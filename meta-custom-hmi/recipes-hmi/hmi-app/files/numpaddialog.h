#ifndef NUMPADDIALOG_H
#define NUMPADDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>

class NumpadDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NumpadDialog(const QString &title, const QString &initialValue, QWidget *parent = nullptr);
    QString getValue() const;

private slots:
    void onKeyClicked();
    void onBackspace();
    void onClear();

private:
    QLineEdit *m_lineEdit;
    QString m_value;
};

#endif // NUMPADDIALOG_H
