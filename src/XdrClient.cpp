#include "XdrClient.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QSettings>
#include <QStringList>
#include <QtGlobal>

namespace {
constexpr qsizetype XdrSaltLength = 16;
constexpr int MinimumFmFrequencyKhz = 87500;
constexpr int MaximumFmFrequencyKhz = 108000;
constexpr int RdsTimeoutMs = 2600;

int boundedBandwidth(int hz)
{
    return qBound(0, hz, 400000);
}
}

XdrClient::XdrClient(QObject *parent) : QObject(parent)
{
    QSettings settings;
    const int savedFrequency =
        settings.value(QStringLiteral("radio/lastFrequencyKhz"),
                       MinimumFmFrequencyKhz).toInt();
    frequencyKhz_ = qBound(MinimumFmFrequencyKhz,
                           savedFrequency,
                           MaximumFmFrequencyKhz);

    smallStepKhz_ = qBound(
        10, settings.value(QStringLiteral("radio/smallStepKhz"), 100).toInt(), 1000);
    largeStepKhz_ = qBound(
        100, settings.value(QStringLiteral("radio/largeStepKhz"), 1000).toInt(), 5000);
    seekThreshold_ = qBound(
        0, settings.value(QStringLiteral("radio/seekThreshold"), 30).toInt(), 80);

    forcedMono_ = settings.value(QStringLiteral("receiver/forcedMono"), false).toBool();
    bandwidthSettingHz_ = boundedBandwidth(
        settings.value(QStringLiteral("receiver/bandwidthHz"), 0).toInt());
    deemphasis_ = qBound(
        0, settings.value(QStringLiteral("receiver/deemphasis"), 0).toInt(), 2);
    agc_ = qBound(0, settings.value(QStringLiteral("receiver/agc"), 2).toInt(), 3);
    rfGain_ = settings.value(QStringLiteral("receiver/rfGain"), false).toBool();
    ifGain_ = settings.value(QStringLiteral("receiver/ifGain"), false).toBool();

    residualTimer_.setSingleShot(true);
    residualTimer_.setInterval(100);

    authenticationTimer_.setSingleShot(true);
    authenticationTimer_.setInterval(3000);

    seekEvaluationTimer_.setSingleShot(true);
    seekEvaluationTimer_.setInterval(650);

    rdsTimeoutTimer_.setSingleShot(true);
    rdsTimeoutTimer_.setInterval(RdsTimeoutMs);

    connect(&socket_, &QTcpSocket::connected, this, &XdrClient::onConnected);
    connect(&socket_, &QTcpSocket::disconnected, this, &XdrClient::onDisconnected);
    connect(&socket_, &QTcpSocket::readyRead, this, &XdrClient::onReadyRead);
    connect(&socket_, &QTcpSocket::errorOccurred, this, &XdrClient::onError);
    connect(&residualTimer_, &QTimer::timeout,
            this, &XdrClient::processResidualBuffer);
    connect(&authenticationTimer_, &QTimer::timeout,
            this, &XdrClient::onAuthenticationTimeout);
    connect(&seekEvaluationTimer_, &QTimer::timeout,
            this, &XdrClient::evaluateSeekStep);
    connect(&rdsTimeoutTimer_, &QTimer::timeout,
            this, &XdrClient::onRdsTimeout);
}

bool XdrClient::connected() const
{
    return socket_.state() == QAbstractSocket::ConnectedState;
}

bool XdrClient::ready() const { return ready_; }
QString XdrClient::statusText() const { return statusText_; }
int XdrClient::frequencyKhz() const { return frequencyKhz_; }
bool XdrClient::signalAvailable() const { return signalAvailable_; }
double XdrClient::signalLevel() const { return signalLevel_; }
int XdrClient::cci() const { return cci_; }
int XdrClient::aci() const { return aci_; }
bool XdrClient::stereo() const { return stereo_; }
bool XdrClient::forcedMono() const { return forcedMono_; }
int XdrClient::bandwidthHz() const { return bandwidthHz_; }
int XdrClient::bandwidthSettingHz() const { return bandwidthSettingHz_; }
int XdrClient::deemphasis() const { return deemphasis_; }
int XdrClient::agc() const { return agc_; }
bool XdrClient::rfGain() const { return rfGain_; }
bool XdrClient::ifGain() const { return ifGain_; }
bool XdrClient::rdsActive() const { return rdsActive_; }
QString XdrClient::piCode() const { return piCode_; }
QString XdrClient::psText() const { return psText_; }
QString XdrClient::radioText() const { return radioText_; }
int XdrClient::ptyCode() const { return ptyCode_; }
QString XdrClient::ptyText() const { return ptyText_; }
int XdrClient::rdsGroupCount() const { return rdsGroupCount_; }
QString XdrClient::lastLine() const { return lastLine_; }
int XdrClient::minimumFmFrequencyKhz() const { return MinimumFmFrequencyKhz; }
int XdrClient::maximumFmFrequencyKhz() const { return MaximumFmFrequencyKhz; }
int XdrClient::smallStepKhz() const { return smallStepKhz_; }
int XdrClient::largeStepKhz() const { return largeStepKhz_; }
int XdrClient::seekThreshold() const { return seekThreshold_; }
bool XdrClient::seeking() const { return seeking_; }
int XdrClient::seekDirection() const { return seekDirection_; }

QString XdrClient::receptionModeText() const
{
    if (forcedMono_)
        return stereo_ ? QStringLiteral("MONO erzwungen · Stereosignal")
                       : QStringLiteral("MONO erzwungen");
    return stereo_ ? QStringLiteral("STEREO") : QStringLiteral("MONO");
}

void XdrClient::connectToServer(const QString &host, int port,
                                const QString &password)
{
    residualTimer_.stop();
    authenticationTimer_.stop();
    rdsTimeoutTimer_.stop();
    cancelSeekSilently();
    socket_.abort();
    buffer_.clear();
    pendingSalt_.clear();
    authenticationSent_ = false;
    startupSent_ = false;
    password_ = password;
    setReady(false);
    clearSignalData();
    clearRdsData();

    const QString cleanHost = host.trimmed();
    if (cleanHost.isEmpty() || port < 1 || port > 65535) {
        setStatusText(QStringLiteral("Ungültige IP-Adresse oder Portnummer"));
        return;
    }

    setStatusText(QStringLiteral("Verbinde mit %1:%2 …").arg(cleanHost).arg(port));
    qInfo().noquote() << "TCP CONNECT" << cleanHost << port;
    socket_.connectToHost(cleanHost, static_cast<quint16>(port));
}

void XdrClient::disconnectFromServer()
{
    authenticationTimer_.stop();
    rdsTimeoutTimer_.stop();
    cancelSeekSilently();
    socket_.disconnectFromHost();
}

void XdrClient::setFrequencyKhz(int khz)
{
    if (seeking_)
        finishSeek(QStringLiteral("Suchlauf durch manuelle Abstimmung beendet"));

    const int bounded = qBound(MinimumFmFrequencyKhz, khz, MaximumFmFrequencyKhz);
    if (bounded == frequencyKhz_) {
        if (khz < MinimumFmFrequencyKhz)
            setStatusText(QStringLiteral("Untere Bandgrenze: 87,500 MHz"));
        else if (khz > MaximumFmFrequencyKhz)
            setStatusText(QStringLiteral("Obere Bandgrenze: 108,000 MHz"));
        return;
    }
    sendFrequencyCommand(bounded);
}

void XdrClient::stepFrequency(int deltaKhz)
{
    if (deltaKhz != 0)
        setFrequencyKhz(frequencyKhz_ + deltaKhz);
}

void XdrClient::stepSmall(int direction)
{
    stepFrequency((direction < 0 ? -1 : 1) * smallStepKhz_);
}

void XdrClient::stepLarge(int direction)
{
    stepFrequency((direction < 0 ? -1 : 1) * largeStepKhz_);
}

void XdrClient::setSmallStepKhz(int khz)
{
    const int value = qBound(10, khz, 1000);
    if (smallStepKhz_ == value)
        return;
    smallStepKhz_ = value;
    QSettings().setValue(QStringLiteral("radio/smallStepKhz"), value);
    emit tuningSettingsChanged();
}

void XdrClient::setLargeStepKhz(int khz)
{
    const int value = qBound(100, khz, 5000);
    if (largeStepKhz_ == value)
        return;
    largeStepKhz_ = value;
    QSettings().setValue(QStringLiteral("radio/largeStepKhz"), value);
    emit tuningSettingsChanged();
}

void XdrClient::setSeekThreshold(int value)
{
    value = qBound(0, value, 80);
    if (seekThreshold_ == value)
        return;
    seekThreshold_ = value;
    QSettings().setValue(QStringLiteral("radio/seekThreshold"), value);
    emit tuningSettingsChanged();
}

void XdrClient::startSeek(int direction)
{
    if (!ready_) {
        setStatusText(QStringLiteral("Tuner ist noch nicht bereit"));
        return;
    }
    if (direction != -1 && direction != 1)
        return;

    const bool stateChanged = !seeking_ || seekDirection_ != direction;
    seeking_ = true;
    seekDirection_ = direction;
    seekSignalSum_ = 0.0;
    seekSignalSamples_ = 0;
    if (stateChanged)
        emit seekingChanged();

    setStatusText(direction > 0
                      ? QStringLiteral("Suchlauf aufwärts …")
                      : QStringLiteral("Suchlauf abwärts …"));
    advanceSeek();
}

void XdrClient::stopSeek()
{
    if (seeking_)
        finishSeek(QStringLiteral("Suchlauf gestoppt"));
}

void XdrClient::setForcedMono(bool enabled)
{
    if (forcedMono_ != enabled) {
        forcedMono_ = enabled;
        QSettings().setValue(QStringLiteral("receiver/forcedMono"), enabled);
        emit receptionModeChanged();
    }
    // XDR-GTK: B1 = Mono erzwingen, B0 = Stereo-Automatik.
    sendLine(enabled ? QStringLiteral("B1") : QStringLiteral("B0"));
}

void XdrClient::setStereoAuto(bool enabled)
{
    setForcedMono(!enabled);
}

void XdrClient::setBandwidth(int hz)
{
    const int value = boundedBandwidth(hz);
    if (bandwidthSettingHz_ != value) {
        bandwidthSettingHz_ = value;
        QSettings().setValue(QStringLiteral("receiver/bandwidthHz"), value);
        emit receiverSettingsChanged();
    }
    sendLine(QStringLiteral("W%1").arg(value));
}

void XdrClient::setDeemphasis(int mode)
{
    const int value = qBound(0, mode, 2);
    if (deemphasis_ != value) {
        deemphasis_ = value;
        QSettings().setValue(QStringLiteral("receiver/deemphasis"), value);
        emit receiverSettingsChanged();
    }
    sendLine(QStringLiteral("D%1").arg(value));
}

void XdrClient::setAgc(int mode)
{
    const int value = qBound(0, mode, 3);
    if (agc_ != value) {
        agc_ = value;
        QSettings().setValue(QStringLiteral("receiver/agc"), value);
        emit receiverSettingsChanged();
    }
    sendLine(QStringLiteral("A%1").arg(value));
}

void XdrClient::setRfGain(bool enabled)
{
    if (rfGain_ == enabled)
        return;
    rfGain_ = enabled;
    QSettings().setValue(QStringLiteral("receiver/rfGain"), enabled);
    emit receiverSettingsChanged();
    sendGainCommand();
}

void XdrClient::setIfGain(bool enabled)
{
    if (ifGain_ == enabled)
        return;
    ifGain_ = enabled;
    QSettings().setValue(QStringLiteral("receiver/ifGain"), enabled);
    emit receiverSettingsChanged();
    sendGainCommand();
}

void XdrClient::onConnected()
{
    emit connectedChanged();
    setStatusText(QStringLiteral("TCP verbunden – warte auf Anmeldekennung"));
    authenticationTimer_.start();
    qInfo() << "TCP CONNECTED";
}

void XdrClient::onDisconnected()
{
    residualTimer_.stop();
    authenticationTimer_.stop();
    rdsTimeoutTimer_.stop();
    buffer_.clear();
    pendingSalt_.clear();
    authenticationSent_ = false;
    startupSent_ = false;
    cancelSeekSilently();
    const bool wasReady = ready_;
    setReady(false);
    clearSignalData();
    clearRdsData();
    emit connectedChanged();

    if (socket_.error() == QAbstractSocket::RemoteHostClosedError && !wasReady)
        setStatusText(QStringLiteral("Anmeldung abgelehnt – Passwort prüfen"));
    else
        setStatusText(QStringLiteral("Nicht verbunden"));

    qInfo() << "TCP DISCONNECTED";
}

void XdrClient::onReadyRead()
{
    const QByteArray data = socket_.readAll();
    if (data.isEmpty())
        return;

    qInfo().noquote() << "RX RAW:"
                      << QString::fromUtf8(data)
                             .replace('\r', "\\r")
                             .replace('\n', "\\n");
    buffer_ += data;

    if (!authenticationSent_ && buffer_.size() >= XdrSaltLength) {
        pendingSalt_ = buffer_.left(XdrSaltLength);
        buffer_.remove(0, XdrSaltLength);
        if (buffer_.startsWith("\r\n"))
            buffer_.remove(0, 2);
        else if (buffer_.startsWith('\n'))
            buffer_.remove(0, 1);
        sendAuthentication(pendingSalt_);
    }

    processCompleteLines();

    if (!buffer_.isEmpty())
        residualTimer_.start();
}

void XdrClient::processCompleteLines()
{
    while (true) {
        const qsizetype newline = buffer_.indexOf('\n');
        if (newline < 0)
            return;

        QByteArray raw = buffer_.left(newline);
        buffer_.remove(0, newline + 1);
        raw.replace("\r", "");

        const QString line = QString::fromUtf8(raw).trimmed();
        if (!line.isEmpty())
            processLine(line);
    }
}

void XdrClient::processResidualBuffer()
{
    if (buffer_.isEmpty())
        return;

    if (!authenticationSent_) {
        if (buffer_.size() >= XdrSaltLength) {
            pendingSalt_ = buffer_.left(XdrSaltLength);
            buffer_.remove(0, XdrSaltLength);
            sendAuthentication(pendingSalt_);
        }
        return;
    }

    QByteArray raw = buffer_;
    buffer_.clear();
    raw.replace("\r", "");
    raw.replace("\n", "");

    const QString line = QString::fromUtf8(raw).trimmed();
    if (!line.isEmpty())
        processLine(line);
}

void XdrClient::onAuthenticationTimeout()
{
    if (!authenticationSent_) {
        setStatusText(QStringLiteral("Keine Anmeldekennung vom Server empfangen"));
        socket_.disconnectFromHost();
    }
}

void XdrClient::onError(QAbstractSocket::SocketError error)
{
    cancelSeekSilently();
    setReady(false);

    if (error == QAbstractSocket::RemoteHostClosedError && authenticationSent_)
        setStatusText(QStringLiteral("Anmeldung abgelehnt – Passwort prüfen"));
    else
        setStatusText(QStringLiteral("TCP-Fehler: %1").arg(socket_.errorString()));

    emit connectedChanged();
    qWarning().noquote() << "TCP ERROR:" << socket_.errorString();
}

void XdrClient::sendAuthentication(const QByteArray &salt)
{
    if (authenticationSent_ || salt.size() != XdrSaltLength)
        return;

    authenticationTimer_.stop();

    QByteArray input = salt;
    input += password_.toUtf8();
    const QByteArray digest =
        QCryptographicHash::hash(input, QCryptographicHash::Sha1).toHex();

    authenticationSent_ = true;
    qInfo().noquote() << "AUTH SALT:" << QString::fromLatin1(salt);
    qInfo().noquote() << "AUTH SHA1:" << QString::fromLatin1(digest);
    socket_.write(digest + '\n');
    socket_.flush();

    setStatusText(QStringLiteral("Anmeldung gesendet – warte auf Server"));
}

void XdrClient::sendLine(const QString &line, bool requireReady)
{
    if (!connected()) {
        setStatusText(QStringLiteral("Keine TCP-Verbindung"));
        return;
    }
    if (requireReady && !ready_) {
        setStatusText(QStringLiteral("Tuner ist noch nicht bereit"));
        return;
    }

    const QByteArray packet = line.toUtf8() + '\n';
    qInfo().noquote() << "TX:" << line;
    socket_.write(packet);
    socket_.flush();
}

void XdrClient::processLine(const QString &line)
{
    lastLine_ = line;
    emit lastLineChanged();
    qInfo().noquote() << "RX:" << line;

    if (line == QStringLiteral("a0")) {
        setReady(false);
        setStatusText(QStringLiteral("Anmeldung abgelehnt – Passwort prüfen"));
        return;
    }

    if (line == QStringLiteral("a1")) {
        setReady(false);
        setStatusText(QStringLiteral("Nur Gastzugang – Steuerung nicht erlaubt"));
        return;
    }

    if (line.startsWith('o')) {
        if (!startupSent_) {
            startupSent_ = true;
            setStatusText(QStringLiteral("Anmeldung erfolgreich – starte Tuner"));
            sendLine(QStringLiteral("x"), false);
        }
        return;
    }

    if (line == QStringLiteral("OK")) {
        setReady(true);
        setStatusText(
            QStringLiteral("Tuner bereit – stelle %1 MHz ein")
                .arg(frequencyKhz_ / 1000.0, 0, 'f', 3));
        sendFrequencyCommand(frequencyKhz_);
        applySavedReceiverSettings();
        return;
    }

    if (line.startsWith('T')) {
        const QString frequencyField = line.mid(1).section(',', 0, 0).trimmed();
        bool ok = false;
        const int value = frequencyField.toInt(&ok);
        if (ok && value >= MinimumFmFrequencyKhz && value <= MaximumFmFrequencyKhz) {
            const bool changed = value != frequencyKhz_;
            frequencyKhz_ = value;
            if (changed) {
                clearRdsData();
                emit frequencyChanged();
            }

            if (seeking_) {
                seekSignalSum_ = 0.0;
                seekSignalSamples_ = 0;
                seekEvaluationTimer_.start();
            } else {
                saveFrequency(value);
                setStatusText(QStringLiteral("Tuner bereit"));
            }
        }
        return;
    }

    if (line.startsWith('W')) {
        bool ok = false;
        const int value = line.mid(1).toInt(&ok);
        if (ok) {
            const int normalized = boundedBandwidth(value);
            if (normalized != bandwidthSettingHz_) {
                bandwidthSettingHz_ = normalized;
                QSettings().setValue(QStringLiteral("receiver/bandwidthHz"), normalized);
                emit receiverSettingsChanged();
            }
        }
        return;
    }

    if (line.startsWith('B')) {
        bool ok = false;
        const int value = line.mid(1).toInt(&ok);
        if (ok)
            updateReceptionMode(stereo_, value == 1);
        return;
    }

    if (line.startsWith('D')) {
        bool ok = false;
        const int value = line.mid(1).toInt(&ok);
        if (ok && value >= 0 && value <= 2 && value != deemphasis_) {
            deemphasis_ = value;
            QSettings().setValue(QStringLiteral("receiver/deemphasis"), value);
            emit receiverSettingsChanged();
        }
        return;
    }

    if (line.startsWith('A')) {
        bool ok = false;
        const int value = line.mid(1).toInt(&ok);
        if (ok && value >= 0 && value <= 3 && value != agc_) {
            agc_ = value;
            QSettings().setValue(QStringLiteral("receiver/agc"), value);
            emit receiverSettingsChanged();
        }
        return;
    }

    if (line.startsWith('G')) {
        bool ok = false;
        const int value = line.mid(1).toInt(&ok);
        if (ok) {
            const bool newRf = value == 10 || value == 11;
            const bool newIf = value == 1 || value == 11;
            if (newRf != rfGain_ || newIf != ifGain_) {
                rfGain_ = newRf;
                ifGain_ = newIf;
                QSettings settings;
                settings.setValue(QStringLiteral("receiver/rfGain"), rfGain_);
                settings.setValue(QStringLiteral("receiver/ifGain"), ifGain_);
                emit receiverSettingsChanged();
            }
        }
        return;
    }

    if (line.startsWith('P')) {
        processPiLine(line);
        return;
    }

    if (line.startsWith('R')) {
        processRdsLine(line);
        return;
    }

    if (line.startsWith(QStringLiteral("Ss")) ||
        line.startsWith(QStringLiteral("Sm")) ||
        line.startsWith(QStringLiteral("SS")) ||
        line.startsWith(QStringLiteral("SM"))) {
        const QChar mode = line.at(1);
        const bool receivedStereo = mode == QLatin1Char('s') || mode == QLatin1Char('S');
        const bool forcedMono = mode == QLatin1Char('S') || mode == QLatin1Char('M');
        updateReceptionMode(receivedStereo, forcedMono);

        const QStringList fields = line.mid(2).split(',');
        if (!fields.isEmpty()) {
            bool ok = false;
            const double level = fields.at(0).toDouble(&ok);
            if (ok) {
                const bool changed = !signalAvailable_ ||
                    !qFuzzyCompare(level + 1.0, signalLevel_ + 1.0);
                signalAvailable_ = true;
                signalLevel_ = level;
                if (changed)
                    emit signalChanged();
            }
        }

        bool qualityValuesChanged = false;
        if (fields.size() >= 2) {
            bool ok = false;
            const int value = fields.at(1).toInt(&ok);
            if (ok) {
                const int normalized = qBound(0, value, 100);
                if (normalized != cci_) {
                    cci_ = normalized;
                    qualityValuesChanged = true;
                }
            }
        }
        if (fields.size() >= 3) {
            bool ok = false;
            const int value = fields.at(2).toInt(&ok);
            if (ok) {
                const int normalized = qBound(0, value, 100);
                if (normalized != aci_) {
                    aci_ = normalized;
                    qualityValuesChanged = true;
                }
            }
        }
        if (qualityValuesChanged)
            emit qualityChanged();

        if (fields.size() >= 4) {
            bool ok = false;
            const int bw = fields.at(3).toInt(&ok);
            if (ok && bw >= 0 && bw != bandwidthHz_) {
                bandwidthHz_ = bw;
                emit bandwidthChanged();
            }
        }

        if (seeking_ && signalAvailable_) {
            seekSignalSum_ += signalLevel_;
            seekSignalSamples_++;
        }
        return;
    }
}

void XdrClient::applySavedReceiverSettings()
{
    sendLine(forcedMono_ ? QStringLiteral("B1") : QStringLiteral("B0"));
    sendLine(QStringLiteral("W%1").arg(bandwidthSettingHz_));
    sendLine(QStringLiteral("D%1").arg(deemphasis_));
    sendLine(QStringLiteral("A%1").arg(agc_));
    sendGainCommand();
}

void XdrClient::sendGainCommand()
{
    const int value = (rfGain_ ? 10 : 0) + (ifGain_ ? 1 : 0);
    sendLine(QStringLiteral("G%1").arg(value, 2, 10, QLatin1Char('0')));
}

void XdrClient::clearRdsData()
{
    rdsTimeoutTimer_.stop();
    const bool hadData = rdsActive_ || rdsPi_ >= 0 || !psText_.isEmpty() ||
                         !radioText_.isEmpty() || ptyCode_ >= 0 || rdsGroupCount_ > 0;

    rdsActive_ = false;
    rdsPi_ = -1;
    piCode_ = QStringLiteral("----");
    psText_.clear();
    radioText_.clear();
    ptyCode_ = -1;
    ptyText_.clear();
    rdsGroupCount_ = 0;
    psBuffer_.fill(QLatin1Char(' '), 8);
    psSegments_.fill(false);
    rtBuffer_.fill(QLatin1Char(' '), 64);
    rtSegments_.fill(false);
    rtAbFlagKnown_ = false;
    rtAbFlag_ = false;
    rtVersionB_ = false;

    if (hadData)
        emit rdsChanged();
}

void XdrClient::markRdsActivity()
{
    const bool changed = !rdsActive_;
    rdsActive_ = true;
    rdsTimeoutTimer_.start();
    if (changed)
        emit rdsChanged();
}

void XdrClient::onRdsTimeout()
{
    if (!rdsActive_)
        return;
    rdsActive_ = false;
    emit rdsChanged();
}

void XdrClient::processPiLine(const QString &line)
{
    if (line.size() < 5)
        return;

    markRdsActivity();
    const QString raw = line.mid(1, 4).toUpper();
    bool ok = false;
    const int value = raw.toInt(&ok, 16);
    const QString display = raw;

    bool changed = false;
    if (display != piCode_) {
        piCode_ = display;
        changed = true;
    }
    if (ok && value != rdsPi_) {
        rdsPi_ = value;
        changed = true;
    }
    if (changed)
        emit rdsChanged();
}

void XdrClient::processRdsLine(const QString &line)
{
    const QString payload = line.mid(1).trimmed();
    quint16 blocks[4] = {0, 0, 0, 0};
    quint8 errors = 0;
    bool ok = false;

    if (payload.size() == 18) {
        for (int i = 0; i < 4; ++i) {
            blocks[i] = static_cast<quint16>(payload.mid(i * 4, 4).toUInt(&ok, 16));
            if (!ok)
                return;
        }
        errors = static_cast<quint8>(payload.mid(16, 2).toUInt(&ok, 16));
        if (!ok)
            return;
    } else if (payload.size() == 14) {
        blocks[0] = rdsPi_ >= 0 ? static_cast<quint16>(rdsPi_) : 0;
        for (int i = 1; i < 4; ++i) {
            blocks[i] = static_cast<quint16>(payload.mid((i - 1) * 4, 4).toUInt(&ok, 16));
            if (!ok)
                return;
        }
        const quint8 legacyErrors =
            static_cast<quint8>(payload.mid(12, 2).toUInt(&ok, 16));
        if (!ok)
            return;
        if (!rdsActive_)
            errors |= static_cast<quint8>(0x03U << 6U);
        errors |= static_cast<quint8>((legacyErrors & 0x03U) << 4U);
        errors |= static_cast<quint8>(legacyErrors & 0x0CU);
        errors |= static_cast<quint8>((legacyErrors & 0x30U) >> 4U);
    } else {
        return;
    }

    markRdsActivity();
    ++rdsGroupCount_;
    processRdsGroup(blocks[0], blocks[1], blocks[2], blocks[3], errors);
    emit rdsChanged();
}

void XdrClient::processRdsGroup(quint16 blockA, quint16 blockB,
                                quint16 blockC, quint16 blockD, quint8 errors)
{
    if (rdsBlockError(errors, 0) < 3) {
        const int newPi = static_cast<int>(blockA);
        if (newPi != rdsPi_) {
            rdsPi_ = newPi;
            piCode_ = QStringLiteral("%1").arg(newPi, 4, 16, QLatin1Char('0')).toUpper();
        }
    }

    if (rdsBlockError(errors, 1) >= 3)
        return;

    const int groupType = (blockB >> 12) & 0x0F;
    const bool versionB = ((blockB >> 11) & 0x01) != 0;
    updatePty((blockB >> 5) & 0x1F);

    if (groupType == 0 && rdsBlockError(errors, 3) < 3) {
        updatePsSegment(blockB & 0x03, blockD);
    } else if (groupType == 2 && rdsBlockError(errors, 3) < 3) {
        if (!versionB && rdsBlockError(errors, 2) >= 3)
            return;
        const bool abFlag = ((blockB >> 4) & 0x01) != 0;
        updateRadioTextSegment(versionB, blockB & 0x0F, abFlag, blockC, blockD);
    }
}

void XdrClient::updatePty(int code)
{
    if (code < 0 || code > 31)
        return;
    const QString text = ptyName(code);
    if (ptyCode_ == code && ptyText_ == text)
        return;
    ptyCode_ = code;
    ptyText_ = text;
}

void XdrClient::updatePsSegment(int segment, quint16 blockD)
{
    if (segment < 0 || segment > 3)
        return;

    const int offset = segment * 2;
    psBuffer_[offset] = decodeRdsCharacter(static_cast<quint8>(blockD >> 8));
    psBuffer_[offset + 1] = decodeRdsCharacter(static_cast<quint8>(blockD & 0xFF));
    psSegments_[static_cast<std::size_t>(segment)] = true;

    QString display = psBuffer_;
    while (display.endsWith(QLatin1Char(' ')))
        display.chop(1);
    psText_ = display;
}

void XdrClient::updateRadioTextSegment(bool versionB, int segment, bool abFlag,
                                       quint16 blockC, quint16 blockD)
{
    if (segment < 0 || segment > 15)
        return;

    if (!rtAbFlagKnown_ || rtAbFlag_ != abFlag || rtVersionB_ != versionB) {
        rtBuffer_.fill(QLatin1Char(' '), 64);
        rtSegments_.fill(false);
        radioText_.clear();
        rtAbFlagKnown_ = true;
        rtAbFlag_ = abFlag;
        rtVersionB_ = versionB;
    }

    if (versionB) {
        const int offset = segment * 2;
        rtBuffer_[offset] = decodeRdsCharacter(static_cast<quint8>(blockD >> 8));
        rtBuffer_[offset + 1] = decodeRdsCharacter(static_cast<quint8>(blockD & 0xFF));
    } else {
        const int offset = segment * 4;
        rtBuffer_[offset] = decodeRdsCharacter(static_cast<quint8>(blockC >> 8));
        rtBuffer_[offset + 1] = decodeRdsCharacter(static_cast<quint8>(blockC & 0xFF));
        rtBuffer_[offset + 2] = decodeRdsCharacter(static_cast<quint8>(blockD >> 8));
        rtBuffer_[offset + 3] = decodeRdsCharacter(static_cast<quint8>(blockD & 0xFF));
    }
    rtSegments_[static_cast<std::size_t>(segment)] = true;

    QString display = rtBuffer_;
    const int terminator = display.indexOf(QChar(0x000D));
    if (terminator >= 0)
        display.truncate(terminator);
    while (display.endsWith(QLatin1Char(' ')))
        display.chop(1);
    radioText_ = display;
}

QString XdrClient::ptyName(int code)
{
    static const QStringList names = {
        QStringLiteral("Kein PTY"), QStringLiteral("Nachrichten"),
        QStringLiteral("Aktuelles"), QStringLiteral("Information"),
        QStringLiteral("Sport"), QStringLiteral("Bildung"),
        QStringLiteral("Hörspiel"), QStringLiteral("Kultur"),
        QStringLiteral("Wissenschaft"), QStringLiteral("Verschiedenes"),
        QStringLiteral("Popmusik"), QStringLiteral("Rockmusik"),
        QStringLiteral("Unterhaltungsmusik"), QStringLiteral("Leichte Klassik"),
        QStringLiteral("Ernste Klassik"), QStringLiteral("Sonstige Musik"),
        QStringLiteral("Wetter"), QStringLiteral("Wirtschaft"),
        QStringLiteral("Kinder"), QStringLiteral("Soziales"),
        QStringLiteral("Religion"), QStringLiteral("Hörertelefon"),
        QStringLiteral("Reisen"), QStringLiteral("Freizeit"),
        QStringLiteral("Jazz"), QStringLiteral("Country"),
        QStringLiteral("Nationale Musik"), QStringLiteral("Oldies"),
        QStringLiteral("Folk"), QStringLiteral("Dokumentation"),
        QStringLiteral("Alarmtest"), QStringLiteral("Alarm")
    };
    return (code >= 0 && code < names.size()) ? names.at(code) : QString();
}

QChar XdrClient::decodeRdsCharacter(quint8 value)
{
    if (value == 0x0D)
        return QChar(0x000D);
    if (value < 0x20)
        return QLatin1Char(' ');
    return QChar::fromLatin1(static_cast<char>(value));
}

int XdrClient::rdsBlockError(quint8 errors, int blockIndex)
{
    if (blockIndex < 0 || blockIndex > 3)
        return 3;
    const int shift = (3 - blockIndex) * 2;
    return (errors >> shift) & 0x03;
}

void XdrClient::sendFrequencyCommand(int khz)
{
    const int bounded = qBound(MinimumFmFrequencyKhz, khz, MaximumFmFrequencyKhz);
    sendLine(QStringLiteral("T%1").arg(bounded));
}

void XdrClient::advanceSeek()
{
    if (!seeking_ || seekDirection_ == 0)
        return;

    const int next = frequencyKhz_ + seekDirection_ * smallStepKhz_;
    if (next < MinimumFmFrequencyKhz || next > MaximumFmFrequencyKhz) {
        finishSeek(seekDirection_ > 0
                       ? QStringLiteral("Obere Bandgrenze 108,000 MHz erreicht")
                       : QStringLiteral("Untere Bandgrenze 87,500 MHz erreicht"));
        return;
    }

    seekSignalSum_ = 0.0;
    seekSignalSamples_ = 0;
    sendFrequencyCommand(next);
}

void XdrClient::evaluateSeekStep()
{
    if (!seeking_)
        return;

    const double average = seekSignalSamples_ > 0
        ? seekSignalSum_ / seekSignalSamples_
        : 0.0;

    if (seekSignalSamples_ > 0 && average >= seekThreshold_) {
        finishSeek(QStringLiteral("Sender bei %1 MHz gefunden · Signal %2")
                       .arg(frequencyKhz_ / 1000.0, 0, 'f', 3)
                       .arg(average, 0, 'f', 2));
        return;
    }

    advanceSeek();
}

void XdrClient::finishSeek(const QString &message)
{
    if (!seeking_)
        return;

    seekEvaluationTimer_.stop();
    seeking_ = false;
    seekDirection_ = 0;
    seekSignalSum_ = 0.0;
    seekSignalSamples_ = 0;
    saveFrequency(frequencyKhz_);
    emit seekingChanged();
    setStatusText(message);
}

void XdrClient::cancelSeekSilently()
{
    seekEvaluationTimer_.stop();
    if (!seeking_ && seekDirection_ == 0)
        return;
    seeking_ = false;
    seekDirection_ = 0;
    seekSignalSum_ = 0.0;
    seekSignalSamples_ = 0;
    emit seekingChanged();
}

void XdrClient::saveFrequency(int khz)
{
    QSettings().setValue(QStringLiteral("radio/lastFrequencyKhz"), khz);
    qInfo() << "FREQUENCY SAVED:" << khz;
}

void XdrClient::setStatusText(const QString &text)
{
    if (statusText_ == text)
        return;
    statusText_ = text;
    emit statusTextChanged();
}

void XdrClient::setReady(bool value)
{
    if (ready_ == value)
        return;
    ready_ = value;
    emit readyChanged();
}

void XdrClient::clearSignalData()
{
    const bool hadSignal = signalAvailable_ || signalLevel_ != 0.0;
    const bool hadQuality = cci_ != -1 || aci_ != -1;
    const bool hadMode = stereo_;
    const bool hadBandwidth = bandwidthHz_ != 0;

    signalAvailable_ = false;
    signalLevel_ = 0.0;
    cci_ = -1;
    aci_ = -1;
    stereo_ = false;
    bandwidthHz_ = 0;

    if (hadSignal)
        emit signalChanged();
    if (hadQuality)
        emit qualityChanged();
    if (hadMode)
        emit receptionModeChanged();
    if (hadBandwidth)
        emit bandwidthChanged();
}

void XdrClient::updateReceptionMode(bool stereo, bool forcedMono)
{
    if (stereo_ == stereo && forcedMono_ == forcedMono)
        return;
    stereo_ = stereo;
    forcedMono_ = forcedMono;
    QSettings().setValue(QStringLiteral("receiver/forcedMono"), forcedMono_);
    emit receptionModeChanged();
}
