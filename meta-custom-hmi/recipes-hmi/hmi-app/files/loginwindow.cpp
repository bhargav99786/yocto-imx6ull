#include "loginwindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QDateTime>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>
#include <QFrame>

// ============================================================
// MarknStamp HMI — Premium Login Screen
// Resolution: 1024 x 600   Dark industrial theme
// ============================================================

LoginWindow::LoginWindow(QWidget *parent)
    : QWidget(parent)
    , m_failedAttempts(0)
    , m_locked(false)
{
    setupUi();

    m_clockTimer = new QTimer(this);
    connect(m_clockTimer, &QTimer::timeout, this, &LoginWindow::updateClock);
    m_clockTimer->start(1000);
    updateClock();
}

LoginWindow::~LoginWindow() {}

void LoginWindow::setupUi()
{
    // ── Root background ──────────────────────────────────────
    setStyleSheet(
        "QWidget#loginRoot {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
        "    stop:0 #050a0f, stop:0.5 #0b1220, stop:1 #060c16);"
        "}"
    );
    setObjectName("loginRoot");

    // ── Outer layout: left branding panel + right PIN panel ──
    QHBoxLayout *rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // ═══════════════════════════════════════════════════════
    // LEFT PANEL — Branding
    // ═══════════════════════════════════════════════════════
    QFrame *leftPanel = new QFrame(this);
    leftPanel->setFixedWidth(480);
    leftPanel->setStyleSheet(
        "QFrame {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
        "    stop:0 #0a1628, stop:1 #060e1a);"
        "  border-right: 1px solid #1a2d45;"
        "}"
    );

    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(50, 40, 50, 40);
    leftLayout->setSpacing(0);

    // Clock at top
    m_clockLabel = new QLabel("00:00", leftPanel);
    m_clockLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_clockLabel->setStyleSheet(
        "font-size: 48px; font-weight: bold; color: #00d2ff;"
        "background: transparent; letter-spacing: 2px;"
    );

    m_dateLabel = new QLabel("Monday, 01 Jan", leftPanel);
    m_dateLabel->setAlignment(Qt::AlignLeft);
    m_dateLabel->setStyleSheet(
        "font-size: 15px; color: #4a6b8a; background: transparent;"
        "letter-spacing: 1px; margin-top: 4px;"
    );

    leftLayout->addWidget(m_clockLabel);
    leftLayout->addWidget(m_dateLabel);
    leftLayout->addStretch(2);

    // Company / product logo area
    QLabel *logoMark = new QLabel("M", leftPanel);
    logoMark->setFixedSize(72, 72);
    logoMark->setAlignment(Qt::AlignCenter);
    logoMark->setStyleSheet(
        "background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
        "  stop:0 #00d2ff, stop:1 #0077cc);"
        "border-radius: 36px;"
        "font-size: 38px; font-weight: 900; color: #ffffff;"
        "letter-spacing: -2px;"
    );

    m_titleLabel = new QLabel("MarknStamp", leftPanel);
    m_titleLabel->setAlignment(Qt::AlignLeft);
    m_titleLabel->setStyleSheet(
        "font-size: 30px; font-weight: 900; color: #ffffff;"
        "background: transparent; letter-spacing: 1px;"
        "margin-top: 16px;"
    );

    m_subtitleLabel = new QLabel("HMI Control System\ni.MX6ULL Industrial Platform", leftPanel);
    m_subtitleLabel->setAlignment(Qt::AlignLeft);
    m_subtitleLabel->setStyleSheet(
        "font-size: 14px; color: #4a7a9b; background: transparent;"
        "line-height: 1.6; margin-top: 8px;"
    );

    leftLayout->addWidget(logoMark);
    leftLayout->addWidget(m_titleLabel);
    leftLayout->addWidget(m_subtitleLabel);
    leftLayout->addStretch(3);

    // Footer
    QLabel *footer = new QLabel("Authorized access only", leftPanel);
    footer->setAlignment(Qt::AlignLeft);
    footer->setStyleSheet(
        "font-size: 12px; color: #253545; background: transparent;"
    );
    leftLayout->addWidget(footer);

    // ═══════════════════════════════════════════════════════
    // RIGHT PANEL — PIN Entry
    // ═══════════════════════════════════════════════════════
    QFrame *rightPanel = new QFrame(this);
    rightPanel->setStyleSheet(
        "QFrame {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #060e1a, stop:1 #040810);"
        "}"
    );

    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(60, 50, 60, 40);
    rightLayout->setSpacing(0);
    rightLayout->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

    // ── Title ──
    QLabel *pinTitle = new QLabel("Enter PIN", rightPanel);
    pinTitle->setAlignment(Qt::AlignCenter);
    pinTitle->setStyleSheet(
        "font-size: 22px; font-weight: bold; color: #c9d9e9;"
        "background: transparent; letter-spacing: 2px;"
        "margin-bottom: 6px;"
    );

    QLabel *pinHint = new QLabel("Touch the keypad to unlock", rightPanel);
    pinHint->setAlignment(Qt::AlignCenter);
    pinHint->setStyleSheet(
        "font-size: 13px; color: #2e4a62; background: transparent;"
        "margin-bottom: 28px;"
    );

    // ── PIN Dots ──
    m_pinDotsLabel = new QLabel("○  ○  ○  ○", rightPanel);
    m_pinDotsLabel->setObjectName("pinDots");
    m_pinDotsLabel->setAlignment(Qt::AlignCenter);
    m_pinDotsLabel->setFixedHeight(44);
    m_pinDotsLabel->setStyleSheet(
        "font-size: 28px; color: #1e3248; background: #0b1828;"
        "border: 1px solid #1a3050; border-radius: 10px;"
        "padding: 4px 20px; letter-spacing: 6px;"
    );

    // ── Status label ──
    m_statusLabel = new QLabel("", rightPanel);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setFixedHeight(24);
    m_statusLabel->setStyleSheet(
        "font-size: 13px; color: #ff5252; background: transparent;"
        "margin-top: 8px;"
    );

    m_attemptsLabel = new QLabel("", rightPanel);
    m_attemptsLabel->setAlignment(Qt::AlignCenter);
    m_attemptsLabel->setFixedHeight(20);
    m_attemptsLabel->setStyleSheet(
        "font-size: 12px; color: #ff9800; background: transparent;"
    );

    // ── Numpad ──
    QFrame *padFrame = new QFrame(rightPanel);
    padFrame->setStyleSheet(
        "QFrame { background: transparent; }"
    );
    QGridLayout *grid = new QGridLayout(padFrame);
    grid->setSpacing(10);
    grid->setContentsMargins(0, 20, 0, 0);

    // Button style helper
    auto makeBtn = [this, rightPanel](const QString &text, bool isAction = false) -> QPushButton* {
        QPushButton *btn = new QPushButton(text, rightPanel);
        btn->setFixedSize(80, 64);
        if (isAction) {
            btn->setStyleSheet(
                "QPushButton {"
                "  background-color: #0e2030;"
                "  color: #4a6b8a; font-size: 18px; font-weight: bold;"
                "  border: 1px solid #1a3050; border-radius: 10px;"
                "}"
                "QPushButton:pressed { background-color: #152840; color: #6a8baa; }"
            );
        } else {
            btn->setStyleSheet(
                "QPushButton {"
                "  background-color: #0f1e2e;"
                "  color: #c9d9e9; font-size: 22px; font-weight: bold;"
                "  border: 1px solid #1e3850; border-radius: 10px;"
                "}"
                "QPushButton:pressed {"
                "  background-color: #00d2ff;"
                "  color: #000000; border: 1px solid #00d2ff;"
                "}"
            );
        }
        return btn;
    };

    // Row 1: 1 2 3
    auto *btn1 = makeBtn("1"); auto *btn2 = makeBtn("2"); auto *btn3 = makeBtn("3");
    // Row 2: 4 5 6
    auto *btn4 = makeBtn("4"); auto *btn5 = makeBtn("5"); auto *btn6 = makeBtn("6");
    // Row 3: 7 8 9
    auto *btn7 = makeBtn("7"); auto *btn8 = makeBtn("8"); auto *btn9 = makeBtn("9");
    // Row 4: CLR 0 ⌫
    auto *btnClear = makeBtn("CLR", true);
    auto *btn0     = makeBtn("0");
    auto *btnBack  = makeBtn("⌫", true);

    grid->addWidget(btn1, 0, 0); grid->addWidget(btn2, 0, 1); grid->addWidget(btn3, 0, 2);
    grid->addWidget(btn4, 1, 0); grid->addWidget(btn5, 1, 1); grid->addWidget(btn6, 1, 2);
    grid->addWidget(btn7, 2, 0); grid->addWidget(btn8, 2, 1); grid->addWidget(btn9, 2, 2);
    grid->addWidget(btnClear, 3, 0); grid->addWidget(btn0, 3, 1); grid->addWidget(btnBack, 3, 2);

    // ── Unlock button ──
    m_loginBtn = new QPushButton("UNLOCK", rightPanel);
    m_loginBtn->setFixedHeight(54);
    m_loginBtn->setStyleSheet(
        "QPushButton {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "    stop:0 #0077cc, stop:1 #00d2ff);"
        "  color: #ffffff; font-size: 18px; font-weight: 900;"
        "  border: none; border-radius: 10px; letter-spacing: 3px;"
        "  margin-top: 12px;"
        "}"
        "QPushButton:pressed { background: #0066bb; }"
        "QPushButton:disabled { background: #0e1e2e; color: #2a4050; }"
    );

    // ── Connect digits ──
    for (QPushButton *btn : {btn1,btn2,btn3,btn4,btn5,btn6,btn7,btn8,btn9,btn0}) {
        connect(btn, &QPushButton::clicked, this, [this, btn]() {
            onDigitPressed(btn->text());
        });
    }
    connect(btnBack,  &QPushButton::clicked, this, &LoginWindow::onBackspacePressed);
    connect(btnClear, &QPushButton::clicked, this, &LoginWindow::onClearPressed);
    connect(m_loginBtn, &QPushButton::clicked, this, &LoginWindow::onLoginPressed);

    // ── Assemble right panel ──
    rightLayout->addStretch(1);
    rightLayout->addWidget(pinTitle);
    rightLayout->addWidget(pinHint);
    rightLayout->addWidget(m_pinDotsLabel);
    rightLayout->addWidget(m_statusLabel);
    rightLayout->addWidget(m_attemptsLabel);
    rightLayout->addWidget(padFrame, 0, Qt::AlignHCenter);
    rightLayout->addWidget(m_loginBtn);
    rightLayout->addStretch(1);

    // ── Assemble root ──
    rootLayout->addWidget(leftPanel);
    rootLayout->addWidget(rightPanel, 1);
}

// ──────────────────────────────────────────────────────────
// Slots
// ──────────────────────────────────────────────────────────

void LoginWindow::onDigitPressed(const QString &digit)
{
    if (m_locked) return;
    if (m_pinInput.length() >= 8) return;  // max PIN length

    m_pinInput += digit;
    updatePinDots();
    m_statusLabel->clear();

    // Auto-login when PIN reaches expected length
    if (m_pinInput.length() == QString(LOGIN_PIN).length()) {
        onLoginPressed();
    }
}

void LoginWindow::onBackspacePressed()
{
    if (m_locked) return;
    if (!m_pinInput.isEmpty()) {
        m_pinInput.chop(1);
        updatePinDots();
    }
}

void LoginWindow::onClearPressed()
{
    if (m_locked) return;
    m_pinInput.clear();
    updatePinDots();
    m_statusLabel->clear();
}

void LoginWindow::onLoginPressed()
{
    if (m_locked) return;

    if (m_pinInput == QString(LOGIN_PIN)) {
        // ✓ Correct PIN
        m_pinDotsLabel->setStyleSheet(
            "font-size: 28px; color: #00d2ff; background: #0b1828;"
            "border: 1px solid #00d2ff; border-radius: 10px;"
            "padding: 4px 20px; letter-spacing: 6px;"
        );
        m_statusLabel->setStyleSheet("font-size: 13px; color: #4caf50; background: transparent; margin-top: 8px;");
        m_statusLabel->setText("✓  Access Granted");
        m_loginBtn->setEnabled(false);

        // Short delay then show main HMI
        QTimer::singleShot(400, this, [this]() {
            emit loginSuccess();
        });
    } else {
        // ✗ Wrong PIN
        m_failedAttempts++;
        m_pinInput.clear();
        updatePinDots();
        shakePinDisplay();

        if (m_failedAttempts >= MAX_LOGIN_ATTEMPTS) {
            m_locked = true;
            showError(QString("Locked — wait 30 seconds"));
            m_attemptsLabel->clear();
            m_lockTimer = new QTimer(this);
            m_lockTimer->setSingleShot(true);
            connect(m_lockTimer, &QTimer::timeout, this, [this]() {
                m_locked = false;
                m_failedAttempts = 0;
                m_statusLabel->clear();
                m_attemptsLabel->clear();
                m_pinDotsLabel->setStyleSheet(
                    "font-size: 28px; color: #1e3248; background: #0b1828;"
                    "border: 1px solid #1a3050; border-radius: 10px;"
                    "padding: 4px 20px; letter-spacing: 6px;"
                );
            });
            m_lockTimer->start(30000);
        } else {
            int remaining = MAX_LOGIN_ATTEMPTS - m_failedAttempts;
            showError("Incorrect PIN");
            m_attemptsLabel->setText(QString("%1 attempt%2 remaining")
                .arg(remaining).arg(remaining == 1 ? "" : "s"));
        }
    }
}

void LoginWindow::updateClock()
{
    QDateTime now = QDateTime::currentDateTime();
    m_clockLabel->setText(now.toString("HH:mm"));
    m_dateLabel->setText(now.toString("dddd, dd MMM yyyy"));
}

void LoginWindow::updatePinDots()
{
    int len    = m_pinInput.length();
    int maxLen = QString(LOGIN_PIN).length();

    QString dots;
    for (int i = 0; i < maxLen; ++i) {
        if (i < len)
            dots += "●";
        else
            dots += "○";
        if (i < maxLen - 1) dots += "  ";
    }
    m_pinDotsLabel->setText(dots);
    m_pinDotsLabel->setStyleSheet(
        "font-size: 28px; color: #00d2ff; background: #0b1828;"
        "border: 1px solid #1e3050; border-radius: 10px;"
        "padding: 4px 20px; letter-spacing: 6px;"
    );
}

void LoginWindow::showError(const QString &msg)
{
    m_statusLabel->setStyleSheet(
        "font-size: 13px; color: #ff5252; background: transparent; margin-top: 8px;"
    );
    m_statusLabel->setText("✗  " + msg);
    m_pinDotsLabel->setStyleSheet(
        "font-size: 28px; color: #ff5252; background: #0b1828;"
        "border: 1px solid #ff3a3a; border-radius: 10px;"
        "padding: 4px 20px; letter-spacing: 6px;"
    );
}

void LoginWindow::shakePinDisplay()
{
    // Horizontal shake animation on pin dots
    QPropertyAnimation *anim = new QPropertyAnimation(m_pinDotsLabel, "pos");
    anim->setDuration(300);
    QPoint orig = m_pinDotsLabel->pos();
    anim->setKeyValueAt(0.0,  orig);
    anim->setKeyValueAt(0.15, orig + QPoint(-10, 0));
    anim->setKeyValueAt(0.30, orig + QPoint(10, 0));
    anim->setKeyValueAt(0.45, orig + QPoint(-8, 0));
    anim->setKeyValueAt(0.60, orig + QPoint(8, 0));
    anim->setKeyValueAt(0.75, orig + QPoint(-4, 0));
    anim->setKeyValueAt(1.0,  orig);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}
