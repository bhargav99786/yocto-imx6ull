#include "numpaddialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>

NumpadDialog::NumpadDialog(const QString &title, const QString &initialValue, QWidget *parent)
    : QDialog(parent)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setFixedSize(560, 480);
    setStyleSheet("background-color: #111822; border: 2px solid #00d2ff; border-radius: 12px; font-family: sans-serif;");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 16, 18, 16);
    mainLayout->setSpacing(12);

    // Title
    QLabel *titleLabel = new QLabel(title, this);
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #00d2ff; border: none;");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    // Input Line Edit
    m_lineEdit = new QLineEdit(initialValue, this);
    m_lineEdit->setFixedHeight(50);
    m_lineEdit->setStyleSheet("background-color: #1a2432; color: #ffffff; font-size: 20px; font-weight: bold; padding: 0 12px; border: 1px solid #334d66; border-radius: 8px;");
    mainLayout->addWidget(m_lineEdit);

    // Quick presets
    QHBoxLayout *presetLayout = new QHBoxLayout();
    presetLayout->setSpacing(8);
    const QStringList presets = {"http://", "192.168.", "10.0.0.", ":8000"};
    for (const QString &p : presets) {
        QPushButton *btn = new QPushButton(p, this);
        btn->setFixedHeight(38);
        btn->setStyleSheet("background-color: #1e2c3c; color: #82b1ff; font-size: 13px; font-weight: bold; border: 1px solid #2e435c; border-radius: 6px;");
        connect(btn, &QPushButton::clicked, [this, p]() {
            m_lineEdit->insert(p);
        });
        presetLayout->addWidget(btn);
    }
    mainLayout->addLayout(presetLayout);

    // Keypad Grid
    QGridLayout *grid = new QGridLayout();
    grid->setSpacing(8);

    const QString keys[4][4] = {
        {"7", "8", "9", "/"},
        {"4", "5", "6", ":"},
        {"1", "2", "3", "-"},
        {".", "0", "BKSP", "CLR"}
    };

    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            QString key = keys[r][c];
            QPushButton *btn = new QPushButton(key, this);
            btn->setFixedHeight(52);

            if (key == "BKSP") {
                btn->setStyleSheet("background-color: #c62828; color: #ffffff; font-size: 16px; font-weight: bold; border-radius: 8px;");
                connect(btn, &QPushButton::clicked, this, &NumpadDialog::onBackspace);
            } else if (key == "CLR") {
                btn->setStyleSheet("background-color: #ef6c00; color: #ffffff; font-size: 16px; font-weight: bold; border-radius: 8px;");
                connect(btn, &QPushButton::clicked, this, &NumpadDialog::onClear);
            } else {
                btn->setStyleSheet("background-color: #1b2636; color: #ffffff; font-size: 20px; font-weight: bold; border: 1px solid #2b3b50; border-radius: 8px;");
                connect(btn, &QPushButton::clicked, this, &NumpadDialog::onKeyClicked);
            }
            grid->addWidget(btn, r, c);
        }
    }
    mainLayout->addLayout(grid);

    // Bottom Action Buttons
    QHBoxLayout *actionLayout = new QHBoxLayout();
    actionLayout->setSpacing(16);

    QPushButton *cancelBtn = new QPushButton("Cancel", this);
    cancelBtn->setFixedHeight(48);
    cancelBtn->setStyleSheet("background-color: #37474f; color: #eceff1; font-size: 16px; font-weight: bold; border-radius: 8px;");
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    QPushButton *saveBtn = new QPushButton("Save / OK", this);
    saveBtn->setFixedHeight(48);
    saveBtn->setStyleSheet("background-color: #00c853; color: #ffffff; font-size: 16px; font-weight: bold; border-radius: 8px;");
    connect(saveBtn, &QPushButton::clicked, [this]() {
        m_value = m_lineEdit->text().trimmed();
        accept();
    });

    actionLayout->addWidget(cancelBtn);
    actionLayout->addWidget(saveBtn);
    mainLayout->addLayout(actionLayout);
}

QString NumpadDialog::getValue() const
{
    return m_value;
}

void NumpadDialog::onKeyClicked()
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (btn) {
        m_lineEdit->insert(btn->text());
    }
}

void NumpadDialog::onBackspace()
{
    m_lineEdit->backspace();
}

void NumpadDialog::onClear()
{
    m_lineEdit->clear();
}
