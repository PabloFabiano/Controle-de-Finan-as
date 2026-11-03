#include "financemanager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QMap>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

#include <algorithm>

namespace {
const QStringList kCategories = {QStringLiteral("Geral"), QStringLiteral("Alimentação"),
                                 QStringLiteral("Água"), QStringLiteral("Roupas")};

QString currency(double value)
{
    return QLocale(QLocale::Portuguese, QLocale::Brazil).toCurrencyString(value, QStringLiteral("R$"));
}
}

FinanceManager::FinanceManager(QObject *parent)
    : QObject(parent)
{
    const QString dataDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    m_storagePath = QDir(dataDirectory).filePath(QStringLiteral("lancamentos.json"));
    load();
}

QString FinanceManager::statusMessage() const
{
    return m_statusMessage;
}

QString FinanceManager::storagePath() const
{
    return m_storagePath;
}

QVariantList FinanceManager::entriesForMonth(const QString &monthKey) const
{
    QVariantList result;
    QList<Entry> filteredEntries;

    for (const Entry &entry : m_entries) {
        if (entry.date.toString(QStringLiteral("yyyy-MM")) == monthKey) {
            filteredEntries.append(entry);
        }
    }

    std::sort(filteredEntries.begin(), filteredEntries.end(), [](const Entry &first, const Entry &second) {
        return first.date > second.date;
    });

    for (const Entry &entry : filteredEntries) {
        result.append(entryToMap(entry));
    }
    return result;
}

QVariantMap FinanceManager::summaryForMonth(const QString &monthKey) const
{
    QMap<QString, double> expensesByCategory;
    QMap<QString, double> waterByUnit;
    QMap<QString, double> consumptionByCategoryAndUnit;
    double totalAmount = 0.0;
    int entryCount = 0;

    for (const Entry &entry : m_entries) {
        if (entry.date.toString(QStringLiteral("yyyy-MM")) != monthKey) {
            continue;
        }

        ++entryCount;
        if (entry.hasAmount) {
            totalAmount += entry.amount;
            expensesByCategory[entry.category] += entry.amount;
        }
        if (entry.hasQuantity) {
            const QString unit = entry.unit.isEmpty() ? QStringLiteral("un") : entry.unit;
            if (entry.category == QStringLiteral("Água")) {
                waterByUnit[unit] += entry.quantity;
            } else {
                consumptionByCategoryAndUnit[entry.category + QStringLiteral("|") + unit] += entry.quantity;
            }
        }
    }

    QVariantList expenses;
    for (const QString &category : kCategories) {
        if (expensesByCategory.contains(category)) {
            const double value = expensesByCategory.value(category);
            expenses.append(QVariantMap{{QStringLiteral("category"), category},
                                        {QStringLiteral("amount"), value},
                                        {QStringLiteral("amountDisplay"), currency(value)}});
        }
    }

    QVariantList water;
    for (auto it = waterByUnit.cbegin(); it != waterByUnit.cend(); ++it) {
        water.append(QVariantMap{{QStringLiteral("unit"), it.key()},
                                 {QStringLiteral("quantity"), it.value()},
                                 {QStringLiteral("quantityDisplay"), displayNumber(it.value())}});
    }

    QVariantList consumption;
    for (auto it = consumptionByCategoryAndUnit.cbegin(); it != consumptionByCategoryAndUnit.cend(); ++it) {
        const QStringList parts = it.key().split('|');
        consumption.append(QVariantMap{{QStringLiteral("category"), parts.value(0)},
                                       {QStringLiteral("unit"), parts.value(1)},
                                       {QStringLiteral("quantity"), it.value()},
                                       {QStringLiteral("quantityDisplay"), displayNumber(it.value())}});
    }

    return QVariantMap{{QStringLiteral("totalAmount"), totalAmount},
                       {QStringLiteral("totalAmountDisplay"), currency(totalAmount)},
                       {QStringLiteral("entryCount"), entryCount},
                       {QStringLiteral("expensesByCategory"), expenses},
                       {QStringLiteral("waterByUnit"), water},
                       {QStringLiteral("consumptionByCategory"), consumption}};
}

bool FinanceManager::addEntry(const QString &dateText,
                              const QString &category,
                              const QString &description,
                              const QString &amountText,
                              const QString &quantityText,
                              const QString &unit)
{
    const QDate date = parseDate(dateText);
    if (!date.isValid()) {
        setStatusMessage(QStringLiteral("Informe uma data válida no formato dd/MM/aaaa."));
        return false;
    }
    if (!kCategories.contains(category)) {
        setStatusMessage(QStringLiteral("Selecione uma categoria válida."));
        return false;
    }

    Entry entry;
    entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entry.date = date;
    entry.category = category;
    entry.description = description.trimmed().isEmpty() ? QStringLiteral("Sem descrição") : description.trimmed();
    entry.unit = unit.trimmed();

    if (!amountText.trimmed().isEmpty()) {
        if (!parseNumber(amountText, &entry.amount) || entry.amount < 0.0) {
            setStatusMessage(QStringLiteral("Informe um valor monetário válido."));
            return false;
        }
        entry.hasAmount = true;
    }
    if (!quantityText.trimmed().isEmpty()) {
        if (!parseNumber(quantityText, &entry.quantity) || entry.quantity < 0.0) {
            setStatusMessage(QStringLiteral("Informe uma quantidade válida."));
            return false;
        }
        entry.hasQuantity = true;
        if (entry.unit.isEmpty()) {
            entry.unit = QStringLiteral("un");
        }
    }
    if (!entry.hasAmount && !entry.hasQuantity) {
        setStatusMessage(QStringLiteral("Informe um valor, uma quantidade ou ambos."));
        return false;
    }

    m_entries.append(entry);
    if (!save()) {
        m_entries.removeLast();
        return false;
    }

    setStatusMessage(QStringLiteral("Lançamento salvo com sucesso."));
    emit dataChanged();
    return true;
}

bool FinanceManager::removeEntry(const QString &id)
{
    const auto it = std::find_if(m_entries.cbegin(), m_entries.cend(), [&id](const Entry &entry) {
        return entry.id == id;
    });
    if (it == m_entries.cend()) {
        setStatusMessage(QStringLiteral("Lançamento não encontrado."));
        return false;
    }

    const int index = static_cast<int>(std::distance(m_entries.cbegin(), it));
    const Entry removedEntry = m_entries.at(index);
    m_entries.removeAt(index);
    if (!save()) {
        m_entries.insert(index, removedEntry);
        return false;
    }

    setStatusMessage(QStringLiteral("Lançamento removido."));
    emit dataChanged();
    return true;
}

void FinanceManager::load()
{
    QFile file(m_storagePath);
    if (!file.exists()) {
        return;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        setStatusMessage(QStringLiteral("Não foi possível abrir o arquivo de dados."));
        return;
    }

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        setStatusMessage(QStringLiteral("O arquivo de dados está inválido. Nenhum lançamento foi carregado."));
        return;
    }

    const QJsonArray entries = document.object().value(QStringLiteral("entries")).toArray();
    int ignoredEntries = 0;
    for (const QJsonValue &value : entries) {
        if (!value.isObject()) {
            ++ignoredEntries;
            continue;
        }
        const QJsonObject object = value.toObject();
        Entry entry;
        entry.id = object.value(QStringLiteral("id")).toString();
        entry.date = QDate::fromString(object.value(QStringLiteral("date")).toString(), Qt::ISODate);
        entry.category = object.value(QStringLiteral("category")).toString();
        entry.description = object.value(QStringLiteral("description")).toString();
        const QJsonValue amountValue = object.value(QStringLiteral("amount"));
        const QJsonValue quantityValue = object.value(QStringLiteral("quantity"));
        entry.hasAmount = !amountValue.isUndefined();
        entry.amount = amountValue.toDouble();
        entry.hasQuantity = !quantityValue.isUndefined();
        entry.quantity = quantityValue.toDouble();
        entry.unit = object.value(QStringLiteral("unit")).toString();

        if (entry.id.isEmpty() || !entry.date.isValid() || !kCategories.contains(entry.category)
            || (entry.hasAmount && (!amountValue.isDouble() || entry.amount < 0.0))
            || (entry.hasQuantity && (!quantityValue.isDouble() || entry.quantity < 0.0))
            || (!entry.hasAmount && !entry.hasQuantity)) {
            ++ignoredEntries;
            continue;
        }
        m_entries.append(entry);
    }
    if (ignoredEntries > 0) {
        setStatusMessage(QStringLiteral("%1 lançamento(s) inválido(s) foram ignorados.").arg(ignoredEntries));
    }
}

bool FinanceManager::save()
{
    const QFileInfo storageInfo(m_storagePath);
    if (!QDir().mkpath(storageInfo.absolutePath())) {
        setStatusMessage(QStringLiteral("Não foi possível criar a pasta de dados."));
        return false;
    }

    QJsonArray entries;
    for (const Entry &entry : m_entries) {
        QJsonObject object{{QStringLiteral("id"), entry.id},
                           {QStringLiteral("date"), entry.date.toString(Qt::ISODate)},
                           {QStringLiteral("category"), entry.category},
                           {QStringLiteral("description"), entry.description},
                           {QStringLiteral("unit"), entry.unit}};
        if (entry.hasAmount) {
            object.insert(QStringLiteral("amount"), entry.amount);
        }
        if (entry.hasQuantity) {
            object.insert(QStringLiteral("quantity"), entry.quantity);
        }
        entries.append(object);
    }

    QSaveFile file(m_storagePath);
    if (!file.open(QIODevice::WriteOnly)) {
        setStatusMessage(QStringLiteral("Não foi possível salvar os dados."));
        return false;
    }
    const QJsonDocument document(QJsonObject{{QStringLiteral("version"), 1},
                                              {QStringLiteral("entries"), entries}});
    if (file.write(document.toJson(QJsonDocument::Indented)) == -1 || !file.commit()) {
        setStatusMessage(QStringLiteral("Não foi possível concluir o salvamento dos dados."));
        return false;
    }
    return true;
}

void FinanceManager::setStatusMessage(const QString &message)
{
    if (m_statusMessage == message) {
        return;
    }
    m_statusMessage = message;
    emit statusMessageChanged();
}

QDate FinanceManager::parseDate(const QString &text)
{
    const QString trimmed = text.trimmed();
    QDate date = QDate::fromString(trimmed, QStringLiteral("dd/MM/yyyy"));
    if (!date.isValid()) {
        date = QDate::fromString(trimmed, Qt::ISODate);
    }
    return date;
}

bool FinanceManager::parseNumber(const QString &text, double *value)
{
    QString normalized = text.trimmed();
    normalized.remove(' ');
    if (normalized.contains(',')) {
        normalized.remove('.');
        normalized.replace(',', '.');
    }
    bool ok = false;
    const double parsedValue = normalized.toDouble(&ok);
    if (ok) {
        *value = parsedValue;
    }
    return ok;
}

QString FinanceManager::displayNumber(double value)
{
    QString result = QLocale(QLocale::Portuguese, QLocale::Brazil).toString(value, 'f', 2);
    while (result.endsWith(QLatin1Char('0'))) {
        result.chop(1);
    }
    if (result.endsWith(QLatin1Char(','))) {
        result.chop(1);
    }
    return result;
}

QVariantMap FinanceManager::entryToMap(const Entry &entry) const
{
    return QVariantMap{{QStringLiteral("id"), entry.id},
                       {QStringLiteral("date"), entry.date.toString(Qt::ISODate)},
                       {QStringLiteral("dateDisplay"), QLocale(QLocale::Portuguese, QLocale::Brazil).toString(entry.date, QStringLiteral("dd/MM/yyyy"))},
                       {QStringLiteral("category"), entry.category},
                       {QStringLiteral("description"), entry.description},
                       {QStringLiteral("hasAmount"), entry.hasAmount},
                       {QStringLiteral("amount"), entry.amount},
                       {QStringLiteral("amountDisplay"), entry.hasAmount ? currency(entry.amount) : QStringLiteral("—")},
                       {QStringLiteral("hasQuantity"), entry.hasQuantity},
                       {QStringLiteral("quantity"), entry.quantity},
                       {QStringLiteral("quantityDisplay"), entry.hasQuantity ? displayNumber(entry.quantity) : QStringLiteral("—")},
                       {QStringLiteral("unit"), entry.hasQuantity ? entry.unit : QString()}};
}
