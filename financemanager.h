#pragma once

#include <QDate>
#include <QList>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class FinanceManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString storagePath READ storagePath CONSTANT)

public:
    explicit FinanceManager(QObject *parent = nullptr);

    QString statusMessage() const;
    QString storagePath() const;

    Q_INVOKABLE QVariantList entriesForMonth(const QString &monthKey) const;
    Q_INVOKABLE QVariantMap summaryForMonth(const QString &monthKey) const;
    Q_INVOKABLE bool addEntry(const QString &dateText,
                              const QString &category,
                              const QString &description,
                              const QString &amountText,
                              const QString &quantityText,
                              const QString &unit);
    Q_INVOKABLE bool removeEntry(const QString &id);

signals:
    void dataChanged();
    void statusMessageChanged();

private:
    struct Entry {
        QString id;
        QDate date;
        QString category;
        QString description;
        bool hasAmount = false;
        double amount = 0.0;
        bool hasQuantity = false;
        double quantity = 0.0;
        QString unit;
    };

    QList<Entry> m_entries;
    QString m_storagePath;
    QString m_statusMessage;

    void load();
    bool save();
    void setStatusMessage(const QString &message);
    static QDate parseDate(const QString &text);
    static bool parseNumber(const QString &text, double *value);
    static QString displayNumber(double value);
    QVariantMap entryToMap(const Entry &entry) const;
};
