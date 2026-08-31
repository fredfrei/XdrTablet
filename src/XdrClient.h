#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QSerialPort>
#include <QStringList>
#include <QMap>
#include <QSet>
#include <QTimer>
#include <QString>

#include <array>

class QNetworkAccessManager;
class QNetworkReply;

class XdrClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString connectionType READ connectionType NOTIFY connectionTypeChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(int frequencyKhz READ frequencyKhz NOTIFY frequencyChanged)

    Q_PROPERTY(bool signalAvailable READ signalAvailable NOTIFY signalChanged)
    Q_PROPERTY(double signalLevel READ signalLevel NOTIFY signalChanged)
    Q_PROPERTY(int cci READ cci NOTIFY qualityChanged)
    Q_PROPERTY(int aci READ aci NOTIFY qualityChanged)
    Q_PROPERTY(bool stereo READ stereo NOTIFY receptionModeChanged)
    Q_PROPERTY(bool forcedMono READ forcedMono NOTIFY receptionModeChanged)
    Q_PROPERTY(QString receptionModeText READ receptionModeText NOTIFY receptionModeChanged)

    Q_PROPERTY(int bandwidthHz READ bandwidthHz NOTIFY bandwidthChanged)
    Q_PROPERTY(int bandwidthSettingHz READ bandwidthSettingHz NOTIFY receiverSettingsChanged)
    Q_PROPERTY(int deemphasis READ deemphasis NOTIFY receiverSettingsChanged)
    Q_PROPERTY(int agc READ agc NOTIFY receiverSettingsChanged)
    Q_PROPERTY(bool channelEqualizer READ channelEqualizer NOTIFY receiverSettingsChanged)
    Q_PROPERTY(bool multipathSuppression READ multipathSuppression NOTIFY receiverSettingsChanged)

    Q_PROPERTY(bool rdsActive READ rdsActive NOTIFY rdsChanged)
    Q_PROPERTY(QString piCode READ piCode NOTIFY rdsChanged)
    Q_PROPERTY(QString eccCode READ eccCode NOTIFY rdsChanged)
    Q_PROPERTY(QString pinText READ pinText NOTIFY rdsChanged)

    Q_PROPERTY(bool epgAvailable READ epgAvailable NOTIFY epgChanged)
    Q_PROPERTY(QString epgNow READ epgNow NOTIFY epgChanged)
    Q_PROPERTY(QString epgNext READ epgNext NOTIFY epgChanged)
    Q_PROPERTY(QString psText READ psText NOTIFY rdsChanged)
    Q_PROPERTY(QString radioText READ radioText NOTIFY rdsChanged)
    Q_PROPERTY(int ptyCode READ ptyCode NOTIFY rdsChanged)
    Q_PROPERTY(QString ptyText READ ptyText NOTIFY rdsChanged)
    Q_PROPERTY(QString rtPlusTitle READ rtPlusTitle NOTIFY rdsChanged)
    Q_PROPERTY(QString rtPlusArtist READ rtPlusArtist NOTIFY rdsChanged)
    Q_PROPERTY(bool rtPlusItemRunning READ rtPlusItemRunning NOTIFY rdsChanged)
    Q_PROPERTY(bool rtPlusItemRunningKnown READ rtPlusItemRunningKnown NOTIFY rdsChanged)
    Q_PROPERTY(QString ctText READ ctText NOTIFY rdsChanged)
    Q_PROPERTY(bool rdsErrorCorrectionEnabled
               READ rdsErrorCorrectionEnabled
               NOTIFY rdsErrorCorrectionChanged)
    Q_PROPERTY(int rdsGroupCount READ rdsGroupCount NOTIFY rdsChanged)
    Q_PROPERTY(QString rdsGroupStats READ rdsGroupStats NOTIFY rdsChanged)
    Q_PROPERTY(QString rdsErrorStats READ rdsErrorStats NOTIFY rdsChanged)
    Q_PROPERTY(QString rdsRawGroups READ rdsRawGroups NOTIFY rdsChanged)
    Q_PROPERTY(QString rdsFlagsText READ rdsFlagsText NOTIFY rdsChanged)
    Q_PROPERTY(QString rdsAfText READ rdsAfText NOTIFY rdsChanged)
    Q_PROPERTY(QString rdsPtynText READ rdsPtynText NOTIFY rdsChanged)
    Q_PROPERTY(QString rdsLanguageText READ rdsLanguageText NOTIFY rdsChanged)
    Q_PROPERTY(QString rdsOdaText READ rdsOdaText NOTIFY rdsChanged)
    Q_PROPERTY(QString rdsEonText READ rdsEonText NOTIFY rdsChanged)

    // XDRTABLET_TMC_LED_TEST_V1
    Q_PROPERTY(bool tmcActive READ tmcActive NOTIFY tmcChanged)
    Q_PROPERTY(int tmcGroupCount READ tmcGroupCount NOTIFY tmcChanged)
    Q_PROPERTY(int tmcLocationTableNumber READ tmcLocationTableNumber NOTIFY tmcChanged)
    Q_PROPERTY(int tmcServiceId READ tmcServiceId NOTIFY tmcChanged)
    Q_PROPERTY(int tmcSingleCount READ tmcSingleCount NOTIFY tmcChanged)
    // XDRTABLET_TMC_MULTI_V1
    Q_PROPERTY(int tmcMultiCount READ tmcMultiCount NOTIFY tmcChanged)
    Q_PROPERTY(int tmcMultiOrphanCount READ tmcMultiOrphanCount NOTIFY tmcChanged)
    Q_PROPERTY(int tmcMessageCount READ tmcMessageCount NOTIFY tmcChanged)
    Q_PROPERTY(QString tmcMessagesText READ tmcMessagesText NOTIFY tmcChanged)
    Q_PROPERTY(QString tmcLastRaw READ tmcLastRaw NOTIFY tmcChanged)

    Q_PROPERTY(QString lastLine READ lastLine NOTIFY lastLineChanged)
    Q_PROPERTY(int minimumFmFrequencyKhz READ minimumFmFrequencyKhz CONSTANT)
    Q_PROPERTY(int maximumFmFrequencyKhz READ maximumFmFrequencyKhz CONSTANT)
    Q_PROPERTY(int smallStepKhz READ smallStepKhz NOTIFY tuningSettingsChanged)
    Q_PROPERTY(int largeStepKhz READ largeStepKhz NOTIFY tuningSettingsChanged)
    Q_PROPERTY(int seekThreshold READ seekThreshold NOTIFY tuningSettingsChanged)
    Q_PROPERTY(bool seeking READ seeking NOTIFY seekingChanged)
    Q_PROPERTY(int seekDirection READ seekDirection NOTIFY seekingChanged)

public:
    explicit XdrClient(QObject *parent = nullptr);

    bool connected() const;
    QString connectionType() const;
    bool ready() const;
    QString statusText() const;
    int frequencyKhz() const;

    bool signalAvailable() const;
    double signalLevel() const;
    int cci() const;
    int aci() const;
    bool stereo() const;
    bool forcedMono() const;
    QString receptionModeText() const;

    int bandwidthHz() const;
    int bandwidthSettingHz() const;
    int deemphasis() const;
    int agc() const;
    bool channelEqualizer() const;
    bool multipathSuppression() const;

    bool rdsActive() const;
    QString piCode() const;
    QString eccCode() const;
    QString pinText() const;

    bool epgAvailable() const;
    QString epgNow() const;
    QString epgNext() const;
    QString psText() const;
    QString radioText() const;
    int ptyCode() const;
    QString ptyText() const;
    QString rtPlusTitle() const;
    QString rtPlusArtist() const;
    bool rtPlusItemRunning() const;
    bool rtPlusItemRunningKnown() const;
    QString ctText() const;
    bool rdsErrorCorrectionEnabled() const;
    int rdsGroupCount() const;
    QString rdsGroupStats() const;
    QString rdsErrorStats() const;
    QString rdsRawGroups() const;
    QString rdsFlagsText() const;
    QString rdsAfText() const;
    QString rdsPtynText() const;
    QString rdsLanguageText() const;
    QString rdsOdaText() const;
    QString rdsEonText() const;

    bool tmcActive() const;
    int tmcGroupCount() const;
    int tmcLocationTableNumber() const;
    int tmcServiceId() const;
    int tmcSingleCount() const;
    int tmcMultiCount() const;
    int tmcMultiOrphanCount() const;
    int tmcMessageCount() const;
    QString tmcMessagesText() const;
    QString tmcLastRaw() const;

    QString lastLine() const;
    int minimumFmFrequencyKhz() const;
    int maximumFmFrequencyKhz() const;
    int smallStepKhz() const;
    int largeStepKhz() const;
    int seekThreshold() const;
    bool seeking() const;
    int seekDirection() const;

    Q_INVOKABLE void connectToServer(const QString &host, int port,
                                     const QString &password);
    Q_INVOKABLE void connectToUsb(const QString &portName,
                                  int baudRate = 115200);
    Q_INVOKABLE QStringList availableSerialPorts() const;
    Q_INVOKABLE void disconnectFromServer();
    Q_INVOKABLE void setFrequencyKhz(int khz);
    Q_INVOKABLE void stepFrequency(int deltaKhz);
    Q_INVOKABLE void stepSmall(int direction);
    Q_INVOKABLE void stepLarge(int direction);
    Q_INVOKABLE void setSmallStepKhz(int khz);
    Q_INVOKABLE void setLargeStepKhz(int khz);
    Q_INVOKABLE void setSeekThreshold(int value);
    Q_INVOKABLE void startSeek(int direction);
    Q_INVOKABLE void stopSeek();

    Q_INVOKABLE void setForcedMono(bool enabled);
    Q_INVOKABLE void setStereoAuto(bool enabled); // Kompatibilität zur älteren UI
    Q_INVOKABLE void setBandwidth(int hz);
    Q_INVOKABLE void setDeemphasis(int mode);
    Q_INVOKABLE void setAgc(int mode);
    Q_INVOKABLE void setChannelEqualizer(bool enabled);
    Q_INVOKABLE void setMultipathSuppression(bool enabled);
    Q_INVOKABLE void setRdsErrorCorrectionEnabled(bool enabled);
    Q_INVOKABLE void clearRdsMonitor();
    Q_INVOKABLE QString saveRdsMonitor() const;

signals:
    void connectedChanged();
    void connectionTypeChanged();
    void readyChanged();
    void statusTextChanged();
    void frequencyChanged();
    void signalChanged();
    void qualityChanged();
    void receptionModeChanged();
    void bandwidthChanged();
    void receiverSettingsChanged();
    void rdsChanged();
    void tmcChanged();
    void epgChanged();
    void rdsErrorCorrectionChanged();
    void lastLineChanged();
    void tuningSettingsChanged();
    void seekingChanged();

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError(QAbstractSocket::SocketError error);
    void onSerialReadyRead();
    void onSerialError(QSerialPort::SerialPortError error);
    void processResidualBuffer();
    void onAuthenticationTimeout();
    void evaluateSeekStep();
    void onRdsTimeout();
    void pruneStaleTmcMessages();  // XDRTABLET_TMC_ACTIVE_MESSAGES_V1

private:
    void sendLine(const QString &line, bool requireReady = true);
    void processLine(const QString &line);
    void processCompleteLines();
    void sendAuthentication(const QByteArray &salt);
    void setStatusText(const QString &text);
    void setReady(bool value);

    void clearSignalData();
    void updateReceptionMode(bool stereo, bool forcedMono);
    void applySavedReceiverSettings();
    void sendDspCommand();

    void clearEpg();
    void refreshEpg();
    void fetchDlfEpg();
    void updateEpgFromXml();
    static int isoDurationSeconds(const QString &value);

    void clearRdsData();
    void markRdsActivity();
    void processPiLine(const QString &line);
    void processRdsLine(const QString &line);
    void recordRdsMonitorLine(const QString &line,
                              quint16 blockB,
                              quint8 errors);
    void processRdsGroup(quint16 blockA, quint16 blockB,
                         quint16 blockC, quint16 blockD, quint8 errors);
    void updateRdsExtendedMonitor(quint16 blockB,
                                  quint16 blockC,
                                  quint16 blockD,
                                  quint8 errors);
    void updatePty(int code);
    void updatePsSegment(int segment, quint16 blockD);
    void updateRadioTextSegment(bool versionB, int segment, bool abFlag,
                                quint16 blockC, quint16 blockD);
    static QString ptyName(int code);
    static QString rdsLanguageName(int code);
    static QChar decodeRdsCharacter(quint8 value);
    static int rdsBlockError(quint8 errors, int blockIndex);
    void removeTmcMessagesForLocation(int locationCode);
    void rebuildTmcMessageList();
    void upsertTmcMessage(const QString &key,
                          int eventCode,
                          int locationCode,
                          const QString &message);

    void sendFrequencyCommand(int khz);
    void advanceSeek();
    void finishSeek(const QString &message);
    void cancelSeekSilently();
    void saveFrequency(int khz);

    QTcpSocket socket_;
    QSerialPort serial_;
    bool usbMode_ = false;
    QTimer residualTimer_;
    QTimer authenticationTimer_;
    QTimer seekEvaluationTimer_;
    QTimer rdsTimeoutTimer_;
    QTimer tmcCleanupTimer_;

    QByteArray buffer_;
    QByteArray pendingSalt_;
    QString password_;
    QString statusText_ = QStringLiteral("Nicht verbunden");
    QString lastLine_;

    int frequencyKhz_ = 87500;
    double signalLevel_ = 0.0;
    int cci_ = -1;
    int aci_ = -1;
    bool signalAvailable_ = false;
    bool stereo_ = false;
    bool forcedMono_ = false;

    int bandwidthHz_ = 0;
    int bandwidthSettingHz_ = 0;
    int deemphasis_ = 0;
    int agc_ = 2;
    bool channelEqualizerEnabled_ = true;
    bool multipathSuppressionEnabled_ = true;

    bool rdsActive_ = false;
    int rdsPi_ = -1;
    QString piCode_ = QStringLiteral("----");
    int rdsEcc_ = -1;
    QString eccCode_ = QStringLiteral("--");
    QString pinText_;

    std::array<quint64, 32> rdsGroupTypeCounts_{};
    std::array<std::array<quint64, 4>, 3> rdsBlockErrorCounts_{};
    quint64 rdsUnusableBCount_ = 0;
    QStringList rdsRawLines_;

    int rdsTp_ = -1;
    int rdsTa_ = -1;
    int rdsMs_ = -1;

    int rdsDiMask_ = 0;
    int rdsDiSeenMask_ = 0;

    struct RdsAfListInfo {
        int expectedCount = -1;

        // 0 = noch unbekannt
        // 1 = Methode A
        // 2 = Methode B
        int method = 0;

        QSet<int> frequencies;
        QSet<int> sameProgramme;
        QSet<int> regionalProgramme;
    };

    QMap<int, RdsAfListInfo> rdsAfLists_;

    int rdsAfCurrentBaseKhz_ = -1;
    int rdsAfCurrentExpectedCount_ = -1;
    int rdsAfCurrentMethod_ = 0;

    QSet<int> rdsAfOtherCodes_;

    QString rdsPtynBuffer_ = QString(8, QLatin1Char(' '));
    std::array<bool, 2> rdsPtynSegments_{{false, false}};
    bool rdsPtynAbKnown_ = false;
    bool rdsPtynAb_ = false;

    int rdsLanguageCode_ = -1;

    QMap<quint16, int> rdsOdaGroupCode_;
    QMap<quint16, quint16> rdsOdaLastData_;
    QMap<quint16, quint64> rdsOdaCount_;

    // XDRTABLET_TMC_SINGLE_V1
    // TMC/ALERT-C: LED-Test plus Single-Group-Auswertung.
    bool tmcActive_ = false;
    int tmcGroupCount_ = 0;
    // XDRTABLET_TMC_ECL_LCL_V1
    int tmcSingleCount_ = 0;
    int tmcMultiCount_ = 0;
    int tmcMultiOrphanCount_ = 0;
    int tmcLocationTableNumber_ = -1;
    int tmcServiceId_ = -1;
    QStringList tmcMessages_;
    QSet<QString> tmcMessageKeys_;
    QMap<QString, qint64> tmcMessageLastSeenMs_;
    QMap<QString, QString> tmcMessageTextByKey_;
    QStringList tmcMessageOrder_;

    struct TmcMultiState {
        int eventCode = 0;
        int locationCode = 0;
        int direction = 1;
        int extent = 0;
        int expectedMax = -1;
        QMap<int, quint32> parts;
    };

    QMap<int, TmcMultiState> tmcMultiStates_;
    QMap<int, QString> tmcMultiLastSignature_;
    QString tmcLastRaw_;

    struct RdsEonInfo {
        QString ps = QString(8, QLatin1Char(' '));
        std::array<bool, 4> psSeen{{false, false, false, false}};

        int tp = -1;
        int ta = -1;
        int pty = -1;

        QSet<int> afKhz;
        QSet<QString> mappedAf;

        QString linkage;
        QString pin;

        std::array<quint64, 16> variantCounts{};
        QMap<int, quint16> lastVariantData;

        quint64 groups = 0;
        quint64 taBursts = 0;
    };

    QMap<quint16, RdsEonInfo> rdsEon_;

    QNetworkAccessManager *epgNetwork_ = nullptr;
    QNetworkReply *epgReply_ = nullptr;
    QTimer epgRefreshTimer_;
    QByteArray epgXml_;
    QString epgDateKey_;
    QString epgNow_;
    QString epgNext_;
    bool epgAvailable_ = false;
    QString psText_;
    QString radioText_;
    int ptyCode_ = -1;
    QString ptyText_;

    QString rtPlusTitle_;
    QString rtPlusArtist_;
    QString ctText_;
    int rtPlusGroupCode_ = -1;
    int rtPlusItemToggle_ = -1;
    bool rtPlusItemRunning_ = false;
    bool rtPlusItemRunningKnown_ = false;

    bool rdsErrorCorrectionEnabled_ = false;


    int rdsGroupCount_ = 0;
    QString psBuffer_ = QString(8, QLatin1Char(' '));
    std::array<bool, 4> psSegments_{{false, false, false, false}};
    QString rtBuffer_ = QString(64, QLatin1Char(' '));
    std::array<bool, 16> rtSegments_{};
    bool rtAbFlagKnown_ = false;
    bool rtAbFlag_ = false;
    bool rtVersionB_ = false;
    bool rtTextComplete_ = false;


    int smallStepKhz_ = 100;
    int largeStepKhz_ = 1000;
    int seekThreshold_ = 30;
    int seekDirection_ = 0;
    double seekSignalSum_ = 0.0;
    int seekSignalSamples_ = 0;
    bool seeking_ = false;

    bool authenticationSent_ = false;
    bool startupSent_ = false;
    bool ready_ = false;
};
