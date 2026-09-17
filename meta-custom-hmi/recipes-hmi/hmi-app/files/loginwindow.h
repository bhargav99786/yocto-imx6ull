#ifndef LOGINWINDOW_H
#define LOGINWINDOW_H

#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QPropertyAnimation>

// ============================================================
// LoginWindow — MarknStamp HMI Login Screen
// Shown at startup. Emits loginSuccess() when correct PIN entered.
// Default PIN: 1234  (change LOGIN_PIN below)
// ============================================================
#define LOGIN_PIN "1234"
#define MAX_LOGIN_ATTEMPTS 5

class LoginWindow : public QWidget
{
    Q_OBJECT

public:
    explicit LoginWindow(QWidget *parent = nullptr);
    ~LoginWindow();

signals:
    void loginSuccess();

private slots:
    void onDigitPressed(const QString &digit);
    void onBackspacePressed();
    void onLoginPressed();
    void onClearPressed();
    void updateClock();

private:
    void setupUi();
    void shakePinDisplay();
    void updatePinDots();
    void showError(const QString &msg);

    QLabel      *m_clockLabel;
    QLabel      *m_dateLabel;
    QLabel      *m_titleLabel;
    QLabel      *m_subtitleLabel;
    QLabel      *m_pinDotsLabel;
    QLabel      *m_statusLabel;
    QLabel      *m_attemptsLabel;
    QPushButton *m_loginBtn;

    QString      m_pinInput;
    int          m_failedAttempts;
    bool         m_locked;
    QTimer      *m_clockTimer;
    QTimer      *m_lockTimer;
};

#endif // LOGINWINDOW_H
