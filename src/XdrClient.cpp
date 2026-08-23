#include <QDate>
#include "XdrClient.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QSettings>
#include <QSerialPortInfo>
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
    // PE5PVB: G erste Stelle = Channel EQ AUS, zweite Stelle = iMS AUS.
    // Alte XdrTablet-Einstellungen werden einmalig logisch invertiert übernommen.
    channelEqualizerEnabled_ = settings.contains(
        QStringLiteral("receiver/channelEqualizer"))
        ? settings.value(QStringLiteral("receiver/channelEqualizer")).toBool()
        : !settings.value(QStringLiteral("receiver/rfGain"), false).toBool();

    multipathSuppressionEnabled_ = settings.contains(
        QStringLiteral("receiver/multipathSuppression"))
        ? settings.value(QStringLiteral("receiver/multipathSuppression")).toBool()
        : !settings.value(QStringLiteral("receiver/ifGain"), false).toBool();

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

    connect(&serial_, &QSerialPort::readyRead,
            this, &XdrClient::onSerialReadyRead);
    connect(&serial_, &QSerialPort::errorOccurred,
            this, &XdrClient::onSerialError);

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
    if (usbMode_)
        return serial_.isOpen();

    return socket_.state() == QAbstractSocket::ConnectedState;
}

QString XdrClient::connectionType() const
{
    return usbMode_
        ? QStringLiteral("usb")
        : QStringLiteral("tcp");
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
bool XdrClient::channelEqualizer() const
{
    return channelEqualizerEnabled_;
}

bool XdrClient::multipathSuppression() const
{
    return multipathSuppressionEnabled_;
}
bool XdrClient::rdsActive() const { return rdsActive_; }
QString XdrClient::piCode() const { return piCode_; }
QString XdrClient::psText() const { return psText_; }
QString XdrClient::radioText() const { return radioText_; }
int XdrClient::ptyCode() const { return ptyCode_; }
QString XdrClient::ptyText() const { return ptyText_; }
QString XdrClient::rtPlusTitle() const { return rtPlusTitle_; }
QString XdrClient::rtPlusArtist() const { return rtPlusArtist_; }
bool XdrClient::rtPlusItemRunning() const { return rtPlusItemRunning_; }
bool XdrClient::rtPlusItemRunningKnown() const { return rtPlusItemRunningKnown_; }
QString XdrClient::ctText() const { return ctText_; }
bool XdrClient::rdsErrorCorrectionEnabled() const
{
    return rdsErrorCorrectionEnabled_;
}
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
    if (serial_.isOpen())
        serial_.close();

    if (usbMode_) {
        usbMode_ = false;
        emit connectionTypeChanged();
        emit connectedChanged();
    }

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

void XdrClient::connectToUsb(const QString &portName, int baudRate)
{
    residualTimer_.stop();
    authenticationTimer_.stop();
    rdsTimeoutTimer_.stop();
    cancelSeekSilently();

    // Eine eventuell bestehende TCP-Verbindung beenden.
    socket_.abort();

    const bool modeChanged = !usbMode_;
    usbMode_ = true;

    if (modeChanged)
        emit connectionTypeChanged();

    if (serial_.isOpen())
        serial_.close();

    buffer_.clear();
    pendingSalt_.clear();

    // USB benötigt keine TCP-Authentifizierung.
    authenticationSent_ = true;
    startupSent_ = true;

    setReady(false);
    clearSignalData();
    clearRdsData();

    const QString cleanPort = portName.trimmed();

    if (cleanPort.isEmpty()) {
        setStatusText(QStringLiteral("Kein USB-Anschluss ausgewählt"));
        emit connectedChanged();
        return;
    }

    serial_.setPortName(cleanPort);
    serial_.setBaudRate(baudRate);
    serial_.setDataBits(QSerialPort::Data8);
    serial_.setParity(QSerialPort::NoParity);
    serial_.setStopBits(QSerialPort::OneStop);
    serial_.setFlowControl(QSerialPort::NoFlowControl);

    setStatusText(
        QStringLiteral("Öffne USB %1 mit %2 Baud …")
            .arg(cleanPort)
            .arg(baudRate));

    if (!serial_.open(QIODevice::ReadWrite)) {
        setStatusText(
            QStringLiteral("USB-Fehler: %1")
                .arg(serial_.errorString()));

        emit connectedChanged();
        return;
    }

    qInfo().noquote()
        << "USB CONNECT"
        << cleanPort
        << baudRate;

    emit connectedChanged();

    setStatusText(QStringLiteral("USB verbunden – starte Tuner"));

    // Bei deinem Tuner getestet:
    // x -> OK, T..., G..., Ss..., P..., R...
    sendLine(QStringLiteral("x"), false);
}

QStringList XdrClient::availableSerialPorts() const
{
    QStringList ports;

    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts())
        ports.append(info.systemLocation());

    return ports;
}

void XdrClient::disconnectFromServer()
{
    residualTimer_.stop();
    authenticationTimer_.stop();
    rdsTimeoutTimer_.stop();
    cancelSeekSilently();

    if (usbMode_) {
        if (serial_.isOpen())
            serial_.close();

        buffer_.clear();
        pendingSalt_.clear();
        authenticationSent_ = false;
        startupSent_ = false;

        setReady(false);
        clearSignalData();
        clearRdsData();

        emit connectedChanged();
        setStatusText(QStringLiteral("Nicht verbunden"));

        qInfo() << "USB DISCONNECTED";
        return;
    }

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

    if (seeking_)
        return;

    seekEvaluationTimer_.stop();
    seeking_ = true;
    seekDirection_ = direction;
    seekSignalSum_ = 0.0;
    seekSignalSamples_ = 0;
    emit seekingChanged();

    setStatusText(direction > 0
                      ? QStringLiteral("Suchlauf aufwärts …")
                      : QStringLiteral("Suchlauf abwärts …"));

    // PE5PVB: C1 = Suchlauf abwärts, C2 = Suchlauf aufwärts.
    sendLine(direction > 0
                 ? QStringLiteral("C2")
                 : QStringLiteral("C1"));
}

void XdrClient::stopSeek()
{
    if (!seeking_)
        return;

    // Ein T-Befehl beendet den Suchlauf in der PE5PVB-Firmware.
    sendFrequencyCommand(frequencyKhz_);
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

void XdrClient::setChannelEqualizer(bool enabled)
{
    if (channelEqualizerEnabled_ == enabled)
        return;

    channelEqualizerEnabled_ = enabled;
    QSettings().setValue(
        QStringLiteral("receiver/channelEqualizer"), enabled);
    emit receiverSettingsChanged();
    sendDspCommand();
}

void XdrClient::setMultipathSuppression(bool enabled)
{
    if (multipathSuppressionEnabled_ == enabled)
        return;

    multipathSuppressionEnabled_ = enabled;
    QSettings().setValue(
        QStringLiteral("receiver/multipathSuppression"), enabled);
    emit receiverSettingsChanged();
    sendDspCommand();
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

void XdrClient::onSerialReadyRead()
{
    if (!usbMode_)
        return;

    const QByteArray data = serial_.readAll();

    if (data.isEmpty())
        return;

    qInfo().noquote()
        << "USB RX RAW:"
        << QString::fromUtf8(data)
               .replace('\r', "\\r")
               .replace('\n', "\\n");

    buffer_ += data;

    processCompleteLines();

    if (!buffer_.isEmpty())
        residualTimer_.start();
}

void XdrClient::onSerialError(QSerialPort::SerialPortError error)
{
    if (!usbMode_ || error == QSerialPort::NoError)
        return;

    qWarning().noquote()
        << "USB ERROR:"
        << serial_.errorString();

    setStatusText(
        QStringLiteral("USB-Fehler: %1")
            .arg(serial_.errorString()));

    if (error == QSerialPort::ResourceError ||
        error == QSerialPort::DeviceNotFoundError ||
        error == QSerialPort::PermissionError) {

        setReady(false);

        if (serial_.isOpen())
            serial_.close();

        emit connectedChanged();
    }
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
        setStatusText(
            usbMode_
                ? QStringLiteral("Keine USB-Verbindung")
                : QStringLiteral("Keine TCP-Verbindung"));
        return;
    }

    if (requireReady && !ready_) {
        setStatusText(QStringLiteral("Tuner ist noch nicht bereit"));
        return;
    }

    const QByteArray packet = line.toUtf8() + '\n';

    if (usbMode_) {
        qInfo().noquote() << "USB TX:" << line;
        serial_.write(packet);
        serial_.flush();
    } else {
        qInfo().noquote() << "TCP TX:" << line;
        socket_.write(packet);
        socket_.flush();
    }
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

        if (usbMode_) {
            setStatusText(QStringLiteral("Tuner bereit"));
            applySavedReceiverSettings();
            return;
        }

        setStatusText(
            QStringLiteral("Tuner bereit – stelle %1 MHz ein")
                .arg(frequencyKhz_ / 1000.0, 0, 'f', 3));
        sendFrequencyCommand(frequencyKhz_);
        applySavedReceiverSettings();
        return;
    }

        if (line.startsWith('T')) {
        const QString frequencyField =
            line.mid(1).section(',', 0, 0).trimmed();
        bool ok = false;
        const int value = frequencyField.toInt(&ok);

        if (ok &&
            value >= MinimumFmFrequencyKhz &&
            value <= MaximumFmFrequencyKhz) {
            const bool changed = value != frequencyKhz_;
            frequencyKhz_ = value;

            if (changed) {
                clearRdsData();
                emit frequencyChanged();
            }

            saveFrequency(value);

            if (!seeking_)
                setStatusText(QStringLiteral("Tuner bereit"));
        }
        return;
    }

    if (line.startsWith('C')) {
        bool ok = false;
        const int value = line.mid(1).toInt(&ok);

        if (!ok)
            return;

        if (value == 1 || value == 2) {
            const int direction = value == 2 ? 1 : -1;
            const bool changed =
                !seeking_ || seekDirection_ != direction;

            seekEvaluationTimer_.stop();
            seeking_ = true;
            seekDirection_ = direction;
            seekSignalSum_ = 0.0;
            seekSignalSamples_ = 0;

            if (changed)
                emit seekingChanged();

            setStatusText(direction > 0
                              ? QStringLiteral("Suchlauf aufwärts …")
                              : QStringLiteral("Suchlauf abwärts …"));
        } else if (value == 0) {
            if (seeking_) {
                finishSeek(
                    QStringLiteral("Suchlauf beendet bei %1 MHz")
                        .arg(frequencyKhz_ / 1000.0, 0, 'f', 3));
            } else {
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
        const QString value = line.mid(1).rightJustified(
            2, QLatin1Char('0'));

        if (value.size() == 2 &&
            (value.at(0) == QLatin1Char('0') ||
             value.at(0) == QLatin1Char('1')) &&
            (value.at(1) == QLatin1Char('0') ||
             value.at(1) == QLatin1Char('1'))) {

            // PE5PVB: 0 bedeutet Funktion EIN, 1 bedeutet Funktion AUS.
            const bool newEqualizer =
                value.at(0) == QLatin1Char('0');
            const bool newMultipathSuppression =
                value.at(1) == QLatin1Char('0');

            if (newEqualizer != channelEqualizerEnabled_ ||
                newMultipathSuppression !=
                    multipathSuppressionEnabled_) {

                channelEqualizerEnabled_ = newEqualizer;
                multipathSuppressionEnabled_ =
                    newMultipathSuppression;

                QSettings settings;
                settings.setValue(
                    QStringLiteral("receiver/channelEqualizer"),
                    channelEqualizerEnabled_);
                settings.setValue(
                    QStringLiteral("receiver/multipathSuppression"),
                    multipathSuppressionEnabled_);
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
    sendDspCommand();
}

void XdrClient::sendDspCommand()
{
    // PE5PVB:
    // erste Stelle 0 = Channel EQ EIN, 1 = AUS
    // zweite Stelle 0 = iMS EIN,       1 = AUS
    const QChar equalizer =
        channelEqualizerEnabled_
            ? QLatin1Char('0')
            : QLatin1Char('1');

    const QChar multipath =
        multipathSuppressionEnabled_
            ? QLatin1Char('0')
            : QLatin1Char('1');

    sendLine(QStringLiteral("G%1%2")
                 .arg(equalizer)
                 .arg(multipath));
}


void XdrClient::setRdsErrorCorrectionEnabled(bool enabled)
{
    if (rdsErrorCorrectionEnabled_ == enabled)
        return;

    rdsErrorCorrectionEnabled_ = enabled;

    /*
     * Bereits gesammelte RT-Segmente stammen möglicherweise
     * noch von der vorherigen Filtereinstellung.
     * Deshalb Textdaten neu aufbauen.
     */
    rtBuffer_.fill(QLatin1Char(' '), 64);
    rtSegments_.fill(false);

    radioText_.clear();
    rtPlusTitle_.clear();
    rtPlusArtist_.clear();
    ctText_.clear();

    rtPlusGroupCode_ = -1;
    rtPlusItemToggle_ = -1;
    rtPlusItemRunning_ = false;
    rtPlusItemRunningKnown_ = false;

    emit rdsErrorCorrectionChanged();
    emit rdsChanged();

    qInfo().noquote()
        << "RDS-Fehlerkorrektur:"
        << (enabled
            ? "korrigierte Blöcke erlaubt"
            : "nur fehlerfreie Blöcke");
}

void XdrClient::clearRdsData()
{
    rdsTimeoutTimer_.stop();
    const bool hadData = rdsActive_ || rdsPi_ >= 0 || !psText_.isEmpty() ||
                         !radioText_.isEmpty() || ptyCode_ >= 0 ||
                         !rtPlusTitle_.isEmpty() || !rtPlusArtist_.isEmpty() ||
                         !ctText_.isEmpty() || rdsGroupCount_ > 0;

    rdsActive_ = false;
    rdsPi_ = -1;
    piCode_ = QStringLiteral("----");
    psText_.clear();
    radioText_.clear();
    ptyCode_ = -1;
    ptyText_.clear();

    rtPlusTitle_.clear();
    rtPlusArtist_.clear();
    ctText_.clear();
    rtPlusGroupCode_ = -1;
    rtPlusItemToggle_ = -1;
    rtPlusItemRunning_ = false;
    rtPlusItemRunningKnown_ = false;

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

    /*
     * AUS: nur Status 0
     * EIN: Status 0, 1 und 2
     * Status 3 ist immer unbrauchbar.
     */
    const auto rdsTextBlockUsable =
        [&](int blockIndex) -> bool {
            const int error =
                rdsBlockError(errors, blockIndex);

            if (rdsErrorCorrectionEnabled_)
                return error < 3;

            return error == 0;
        };


    /*
     * PI darf weiterhin die vom TEF korrigierten Werte verwenden.
     */
    if (rdsBlockError(errors, 0) < 3) {
        const int newPi =
            static_cast<int>(blockA);

        if (rdsPi_ >= 0 &&
            newPi != rdsPi_) {

            rtPlusTitle_.clear();
            rtPlusArtist_.clear();
            ctText_.clear();

            rtPlusGroupCode_ = -1;
            rtPlusItemToggle_ = -1;
    rtPlusItemRunning_ = false;
    rtPlusItemRunningKnown_ = false;
            rtPlusItemRunning_ = false;
            rtPlusItemRunningKnown_ = false;
        }

        if (newPi != rdsPi_) {
            rdsPi_ = newPi;

            piCode_ =
                QStringLiteral("%1")
                    .arg(newPi,
                         4, 16,
                         QLatin1Char('0'))
                    .toUpper();
        }
    }

    /*
     * Ohne brauchbaren Block B kann die Gruppe generell
     * nicht ausgewertet werden.
     */
    if (rdsBlockError(errors, 1) >= 3)
        return;

    const int groupType =
        (blockB >> 12) & 0x0F;

    const bool versionB =
        ((blockB >> 11) & 0x01) != 0;

    /*
     * PTY bleibt vorerst unverändert:
     * korrigierte B-Blöcke dürfen weiterhin verwendet werden.
     */
    updatePty(
        (blockB >> 5) & 0x1F);

    /*
     * ============================================================
     * PS
     * ============================================================
     *
     * Bleibt ebenfalls unverändert.
     */
    if (groupType == 0 &&
        rdsBlockError(errors, 3) < 3) {

        updatePsSegment(
            blockB & 0x03,
            blockD);
    }

    /*
     * ============================================================
     * RadioText 2A / 2B
     * ============================================================
     *
     * TEST:
     *
     * Nur Fehlerstatus 0 verwenden.
     */
    else if (groupType == 2) {

        /*
         * Block B enthält unter anderem:
         * - Gruppe
         * - A/B-Flag
         * - Segmentnummer
         *
         * Deshalb muss auch B fehlerfrei sein.
         */
        if (!rdsTextBlockUsable(1))
            return;

        /*
         * Block D enthält bei 2A und 2B Text.
         */
        if (!rdsTextBlockUsable(3))
            return;

        /*
         * 2A verwendet zusätzlich Block C.
         */
        if (!versionB &&
            !rdsTextBlockUsable(2))
            return;

        const bool abFlag =
            ((blockB >> 4) & 0x01) != 0;

        updateRadioTextSegment(
            versionB,
            blockB & 0x0F,
            abFlag,
            blockC,
            blockD);
    }

    /*
     * ============================================================
     * RT+ ODA-Ankündigung
     * ============================================================
     *
     * Auch hier nur fehlerfreies B und D.
     */
    if (groupType == 3 &&
        !versionB &&
        rdsTextBlockUsable(1) &&
        rdsTextBlockUsable(3) &&
        blockD == 0x4BD7) {

        const int groupCode =
            blockB & 0x1F;

        if (groupCode !=
            rtPlusGroupCode_) {

            rtPlusGroupCode_ =
                groupCode;

            const int number =
                groupCode >> 1;

            const QChar version =
                (groupCode & 1)
                    ? QLatin1Char('B')
                    : QLatin1Char('A');

            qInfo().noquote()
                << QStringLiteral(
                       "RT+ ODA erkannt: AID 4BD7 -> Gruppe %1%2")
                       .arg(number)
                       .arg(version);
        }
    }

    const int receivedGroupCode =
        (groupType << 1) |
        (versionB ? 1 : 0);

    /*
     * RT+ Item-Status.
     *
     * Item Running muss unabhängig davon ausgewertet werden,
     * ob der zugehörige RadioText bereits vollständig ist.
     *
     * Running = 1 : Programmelement läuft
     * Running = 0 : kein laufendes Programmelement /
     *               Unterbrechung
     */
    const bool receivedRtPlusDataGroup =
        rtPlusGroupCode_ >= 0 &&
        receivedGroupCode == rtPlusGroupCode_ &&
        rdsTextBlockUsable(1) &&
        rdsTextBlockUsable(2) &&
        rdsTextBlockUsable(3);

    if (receivedRtPlusDataGroup) {

        const int statusItemToggle =
            (blockB >> 4) & 0x01;

        const bool statusItemRunning =
            ((blockB >> 3) & 0x01) != 0;

        static int lastRtPlusToggle = -1;
        static int lastRtPlusRunning = -1;

        const bool statusChanged =
            statusItemToggle != lastRtPlusToggle ||
            static_cast<int>(statusItemRunning) !=
                lastRtPlusRunning;

        /*
         * RT+ Toggle sofort synchronisieren.
         *
         * Bisher wurde der Togglewechsel erst in der
         * RT+-Textauswertung behandelt. Zu diesem Zeitpunkt
         * konnte der neue RadioText bereits vollständig
         * empfangen worden sein und wurde dann unnötig
         * wieder verworfen.
         *
         * Jetzt beginnt die RT-Sammlung bereits beim
         * Empfang des neuen Toggle-Zustands neu.
         */
        if (rtPlusItemToggle_ < 0 ||
            statusItemToggle != rtPlusItemToggle_) {

            const bool firstItem =
                rtPlusItemToggle_ < 0;

            rtPlusItemToggle_ =
                statusItemToggle;

            /*
             * Alter Titel gehört nicht mehr zum neuen Item.
             */
            rtPlusTitle_.clear();
            rtPlusArtist_.clear();

            rtTextComplete_ = false;
            rtBuffer_.fill(QLatin1Char(' '), 64);
            rtSegments_.fill(false);

            qInfo().noquote()
                << "RT+ SYNC EARLY:"
                << (firstItem
                        ? "erster Toggle"
                        : "Togglewechsel auf")
                << statusItemToggle;
        }

        rtPlusItemRunningKnown_ = true;

        const bool wasRunning =
            rtPlusItemRunning_;

        rtPlusItemRunning_ =
            statusItemRunning;

        /*
         * RT+ Item Running = 0:
         *
         * Es läuft momentan kein RT+-Programmelement.
         * Titel und Interpret deshalb aus der Anzeige entfernen.
         *
         * Gleichzeitig die interne RT-Sammlung neu beginnen,
         * damit beim nächsten START keine RT+-Positionen auf
         * RadioText einer Unterbrechung angewendet werden.
         */
        if (!statusItemRunning &&
            (wasRunning ||
             !rtPlusTitle_.isEmpty() ||
             !rtPlusArtist_.isEmpty())) {

            rtPlusTitle_.clear();
            rtPlusArtist_.clear();

            rtTextComplete_ = false;
            rtBuffer_.fill(QLatin1Char(' '), 64);
            rtSegments_.fill(false);

            qInfo().noquote()
                << "RT+: STOP - Titelanzeige gelöscht";
        }

        if (statusChanged) {

            qInfo().noquote()
                << "RT+ STATUS:"
                << "Toggle =" << statusItemToggle
                << "| Running ="
                << (statusItemRunning ? 1 : 0)
                << "|"
                << (statusItemRunning
                        ? "START"
                        : "STOP");

            lastRtPlusToggle =
                statusItemToggle;

            lastRtPlusRunning =
                statusItemRunning ? 1 : 0;
        }
    }

    /*
     * ============================================================
     * RT+ Daten
     * ============================================================
     *
     * B, C und D müssen Fehlerstatus 0 besitzen.
     */
    if (receivedRtPlusDataGroup &&
        rtTextComplete_) {

        const int itemToggle =
            (blockB >> 4) & 0x01;

        const bool itemRunning =
            ((blockB >> 3) & 0x01) != 0;

        const int contentType1 =
            ((blockB & 0x0007) << 3) |
            ((blockC >> 13) & 0x0007);

        const int start1 =
            (blockC >> 7) & 0x003F;

        const int length1 =
            (blockC >> 1) & 0x003F;

        const int contentType2 =
            ((blockC & 0x0001) << 5) |
            ((blockD >> 11) & 0x001F);

        const int start2 =
            (blockD >> 5) & 0x003F;

        const int length2 =
            blockD & 0x001F;

        /*
         * RT+ Synchronisation:
         *
         * Ein Wechsel des Item-Toggle bedeutet ein neues
         * Programmelement. Die neuen RT+-Positionsdaten dürfen
         * nicht mehr auf den noch sichtbaren alten RadioText
         * angewendet werden.
         *
         * Deshalb RT-Sammlung neu beginnen und dieses RT+-Paket
         * noch nicht auswerten. Der alte sichtbare RT/RT+-Text
         * bleibt dabei erhalten.
         */
        if (rtPlusItemToggle_ >= 0 &&
            itemToggle != rtPlusItemToggle_) {

            rtPlusItemToggle_ = itemToggle;

            /*
             * Das bisherige Item ist mit dem Togglewechsel
             * beendet. Alten Titel/Interpret deshalb nicht
             * weiter als aktuell anzeigen.
             */
            rtPlusTitle_.clear();
            rtPlusArtist_.clear();

            rtTextComplete_ = false;
            rtBuffer_.fill(QLatin1Char(' '), 64);
            rtSegments_.fill(false);

            qInfo().noquote()
                << "RT+ SYNC:"
                << "Togglewechsel auf"
                << itemToggle
                << "- warte auf vollständigen neuen RadioText";

            return;
        }

        /*
         * Auch beim allerersten RT+-Item zunächst
         * synchronisieren.
         *
         * Beim Programmstart kann radioText_ bereits einen
         * allgemeinen Sendertext enthalten, während die
         * RT+-Positionsdaten schon zum aktuellen Musiktitel
         * gehören.
         */
        if (rtPlusItemToggle_ < 0) {

            rtPlusItemToggle_ = itemToggle;

            rtTextComplete_ = false;
            rtBuffer_.fill(QLatin1Char(' '), 64);
            rtSegments_.fill(false);

            qInfo().noquote()
                << "RT+ SYNC:"
                << "erster Toggle"
                << itemToggle
                << "- warte auf vollständigen RadioText";

            return;
        }

        auto extractText =
            [&](int startPosition,
                int encodedLength)
                -> QString {

                const int count =
                    encodedLength + 1;

                /*
                 * radioText_ wird erst nach einem kompletten
                 * fehlerfreien Durchlauf gesetzt.
                 */
                if (startPosition < 0 ||
                    count <= 0 ||
                    startPosition + count >
                        radioText_.size())
                    return QString();

                const int endPosition =
                    startPosition + count;

                /*
                 * RT+-Positionen dürfen nicht mitten in einem
                 * Wort beginnen oder enden.
                 *
                 * Damit werden Positionsdaten verworfen, die
                 * noch zu einem anderen RadioText gehören.
                 */
                auto isWordCharacter =
                    [](QChar ch) -> bool {
                        return ch.isLetterOrNumber();
                    };

                if (startPosition > 0 &&
                    isWordCharacter(
                        radioText_.at(startPosition - 1)) &&
                    isWordCharacter(
                        radioText_.at(startPosition))) {

                    return QString();
                }

                if (endPosition <
                        radioText_.size() &&
                    isWordCharacter(
                        radioText_.at(endPosition - 1)) &&
                    isWordCharacter(
                        radioText_.at(endPosition))) {

                    return QString();
                }

                return radioText_
                    .mid(startPosition,
                         count)
                    .trimmed();
            };

        QString newTitle;
        QString newArtist;

        bool gotTitle = false;
        bool gotArtist = false;

        auto decodeTag =
            [&](int contentType,
                int startPosition,
                int encodedLength) {

                const QString value =
                    extractText(
                        startPosition,
                        encodedLength);

                if (value.isEmpty())
                    return;

                if (contentType == 1) {
                    newTitle = value;
                    gotTitle = true;
                }

                else if (contentType == 4) {
                    newArtist = value;
                    gotArtist = true;
                }
            };

        if (itemRunning) {

            decodeTag(
                contentType1,
                start1,
                length1);

            decodeTag(
                contentType2,
                start2,
                length2);

            /*
             * Für die Musiktitelanzeige verlangen wir
             * ITEM.TITLE und ITEM.ARTIST gemeinsam.
             */
            if (!gotTitle || !gotArtist) {

                /*
                 * Das Paket passt momentan nicht vollständig
                 * zum RadioText.
                 *
                 * Einen bereits gültigen laufenden Titel
                 * NICHT löschen. Er wird nur bei STOP oder
                 * bei einem neuen Item-Toggle entfernt.
                 */
                qInfo().noquote()
                    << "RT+: unvollständiges Paket - bisherigen Titel behalten";

                return;
            }
        }

        /*
         * KEINE Zweifachbestätigung mehr.
         */
        if (newTitle !=
                rtPlusTitle_ ||
            newArtist !=
                rtPlusArtist_) {

            rtPlusTitle_ =
                newTitle;

            rtPlusArtist_ =
                newArtist;

            qInfo().noquote()
                << "RT+: Titel ="
                << rtPlusTitle_
                << "| Interpret ="
                << rtPlusArtist_
                << "| Toggle ="
                << itemToggle;
        }
    }

    /*
     * ============================================================
     * CT / Clock Time
     * ============================================================
     *
     * B, C und D nur mit Fehlerstatus 0.
     */
    if (groupType == 4 &&
        !versionB &&
        rdsTextBlockUsable(1) &&
        rdsTextBlockUsable(2) &&
        rdsTextBlockUsable(3)) {

        const int mjd =
            ((blockB & 0x0003) << 15) |
            ((blockC >> 1) & 0x7FFF);

        const int utcHour =
            ((blockC & 0x0001) << 4) |
            ((blockD >> 12) &
             0x000F);

        const int utcMinute =
            (blockD >> 6) &
            0x003F;

        int offsetHalfHours =
            blockD & 0x001F;

        if (blockD & 0x0020)
            offsetHalfHours =
                -offsetHalfHours;

        if (utcHour < 24 &&
            utcMinute < 60) {

            QDate date =
                QDate::fromJulianDay(
                    static_cast<qint64>(
                        mjd) +
                    2400001LL);

            /*
             * Die zusätzliche Plausibilitätskontrolle
             * behalten wir trotzdem bei.
             */
            if (date.isValid() &&
                qAbs(
                    date.daysTo(
                        QDate::currentDate()))
                    <= 2 &&
                qAbs(offsetHalfHours)
                    <= 28) {

                const int offsetMinutes =
                    offsetHalfHours * 30;

                int localMinutes =
                    utcHour * 60 +
                    utcMinute +
                    offsetMinutes;

                while (localMinutes < 0) {
                    localMinutes += 1440;
                    date =
                        date.addDays(-1);
                }

                while (localMinutes >=
                       1440) {

                    localMinutes -= 1440;
                    date =
                        date.addDays(1);
                }

                const int localHour =
                    localMinutes / 60;

                const int localMinute =
                    localMinutes % 60;

                const int absOffset =
                    qAbs(offsetMinutes);

                const QString offsetText =
                    QStringLiteral(
                        "UTC%1%2:%3")
                        .arg(
                            offsetMinutes < 0
                            ? QStringLiteral("-")
                            : QStringLiteral("+"))
                        .arg(
                            absOffset / 60,
                            2, 10,
                            QLatin1Char('0'))
                        .arg(
                            absOffset % 60,
                            2, 10,
                            QLatin1Char('0'));

                const QString newCt =
                    QStringLiteral(
                        "%1 %2:%3 (%4)")
                        .arg(
                            date.toString(
                                QStringLiteral(
                                    "dd.MM.yyyy")))
                        .arg(
                            localHour,
                            2, 10,
                            QLatin1Char('0'))
                        .arg(
                            localMinute,
                            2, 10,
                            QLatin1Char('0'))
                        .arg(offsetText);

                if (newCt != ctText_) {
                    ctText_ = newCt;

                    qInfo().noquote()
                        << "RDS CT:"
                        << ctText_;
                }
            }
        }
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

    /*
     * Neuer RadioText:
     * A/B-Flag oder 2A/2B hat gewechselt.
     */
    if (!rtAbFlagKnown_ ||
        rtAbFlag_ != abFlag ||
        rtVersionB_ != versionB) {

        rtBuffer_.fill(QLatin1Char(' '), 64);
        rtSegments_.fill(false);

        /*
         * Den alten sichtbaren RT/RT+ stehen lassen,
         * bis ein vollständiger neuer RadioText vorliegt.
         */
        rtTextComplete_ = false;

        rtAbFlagKnown_ = true;
        rtAbFlag_ = abFlag;
        rtVersionB_ = versionB;
    }

    if (versionB) {
        const int offset = segment * 2;

        rtBuffer_[offset] =
            decodeRdsCharacter(
                static_cast<quint8>(blockD >> 8));

        rtBuffer_[offset + 1] =
            decodeRdsCharacter(
                static_cast<quint8>(blockD & 0xFF));

    } else {
        const int offset = segment * 4;

        rtBuffer_[offset] =
            decodeRdsCharacter(
                static_cast<quint8>(blockC >> 8));

        rtBuffer_[offset + 1] =
            decodeRdsCharacter(
                static_cast<quint8>(blockC & 0xFF));

        rtBuffer_[offset + 2] =
            decodeRdsCharacter(
                static_cast<quint8>(blockD >> 8));

        rtBuffer_[offset + 3] =
            decodeRdsCharacter(
                static_cast<quint8>(blockD & 0xFF));
    }

    rtSegments_[static_cast<std::size_t>(segment)] = true;

    /*
     * Nur EIN vollständiger Durchlauf wird verlangt.
     *
     * 2A = 4 Zeichen je Segment
     * 2B = 2 Zeichen je Segment
     */
    const int charsPerSegment =
        versionB ? 2 : 4;

    const int maximumCharacters =
        versionB ? 32 : 64;

    QString candidate =
        rtBuffer_.left(maximumCharacters);

    const int terminator =
        candidate.indexOf(QChar(0x000D));

    int lastRequiredSegment = 15;

    if (terminator >= 0)
        lastRequiredSegment =
            terminator / charsPerSegment;

    /*
     * Alle Segmente dieses Textes müssen einmal mit
     * Fehlerstatus 0 angekommen sein.
     */
    for (int s = 0;
         s <= lastRequiredSegment;
         ++s) {

        if (!rtSegments_[
                static_cast<std::size_t>(s)])
            return;
    }

    if (terminator >= 0)
        candidate.truncate(terminator);

    while (candidate.endsWith(QLatin1Char(' ')))
        candidate.chop(1);

    /*
     * Der nächste angezeigte Text muss wieder aus einem
     * vollständigen Durchlauf bestehen.
     *
     * Es wird aber NICHT mehr verlangt, dass derselbe Text
     * zweimal empfangen wird.
     */
    rtSegments_.fill(false);

    if (candidate.isEmpty())
        return;

    /*
     * Ab jetzt darf RT+ wieder auf diesen RadioText zugreifen.
     */
    rtTextComplete_ = true;

    if (candidate != radioText_) {
        radioText_ = candidate;

        qInfo().noquote()
            << "RDS RT:" << radioText_;
    }
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
    /*
     * RDS verwendet nicht ISO-8859-1/Latin-1, sondern
     * den eigenen RDS-G0-Zeichensatz.
     */

    // 0x0D beendet RadioText.
    if (value == 0x0D)
        return QChar(0x000D);

    // Andere Steuerzeichen nicht anzeigen.
    if (value < 0x20)
        return QLatin1Char(' ');

    /*
     * Auch im ASCII-Bereich unterscheiden sich einige
     * Zeichen vom normalen ASCII/Latin-1.
     */
    if (value < 0x80) {
        switch (value) {
        case 0x24: return QChar(0x00A4); // ¤
        case 0x5E: return QChar(0x2015); // ―
        case 0x60: return QChar(0x2551); // ║
        case 0x7E: return QChar(0x00AF); // ¯
        case 0x7F: return QChar(0x0132); // Ĳ
        default:
            return QChar(value);
        }
    }

    /*
     * RDS G0, Codes 0x80 ... 0xFF.
     * Ein Eintrag 0x0000 wird als Leerzeichen behandelt.
     */
    static const ushort table[128] = {
        0x00E1,0x00E0,0x00E9,0x00E8,0x00ED,0x00EC,0x00F3,0x00F2,
        0x00FA,0x00F9,0x00D1,0x00C7,0x015E,0x00DF,0x00A1,0x0133,
        0x00E2,0x00E4,0x00EA,0x00EB,0x00EE,0x00EF,0x00F4,0x00F6,
        0x00FB,0x00FC,0x00F1,0x00E7,0x015F,0x011F,0x0131,0x2193,

        0x00AA,0x03B1,0x00A9,0x2030,0x011E,0x011B,0x0148,0x0151,
        0x03C0,0x20AC,0x00A3,0x0024,0x2190,0x2191,0x2192,0x00A7,
        0x00BA,0x00B9,0x00B2,0x00B3,0x00B1,0x0130,0x0144,0x0171,
        0x00B5,0x00BF,0x00F7,0x00B0,0x00BC,0x00BD,0x00BE,0x013F,

        0x00C1,0x00C0,0x00C9,0x00C8,0x00CD,0x00CC,0x00D3,0x00D2,
        0x00DA,0x00D9,0x0158,0x010C,0x0160,0x017D,0x0110,0x0140,
        0x00C2,0x00C4,0x00CA,0x00CB,0x00CE,0x00CF,0x00D4,0x00D6,
        0x00DB,0x00DC,0x0159,0x010D,0x0161,0x017E,0x0111,0x00F0,

        0x00C3,0x00C5,0x00C6,0x0152,0x0177,0x00DD,0x00D5,0x00D8,
        0x00DE,0x014A,0x0154,0x0106,0x015A,0x0179,0x0166,0x0000,
        0x00E3,0x00E5,0x00E6,0x0153,0x0175,0x00FD,0x00F5,0x00F8,
        0x00FE,0x014B,0x0155,0x0107,0x015B,0x017A,0x0167,0x0000
    };

    const ushort unicode = table[value - 0x80];

    if (unicode == 0x0000)
        return QLatin1Char(' ');

    return QChar(unicode);
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
