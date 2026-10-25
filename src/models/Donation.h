#pragma once

#include <QString>
#include <QDateTime>

struct Donation
{
    QString id;
    QString platform;      // "Twitch", "YouTube", "Kick", "TikTok"
    QString username;
    QString displayName;
    double amount = 0.0;
    QString currency;      // "$", "USD", "Bits", "Coins", "Roses"
    QString type;          // "Super Chat", "Super Sticker", "Bits", "Gift", "Subscription", "Membership"
    QString message;
    QDateTime timestamp;
    
    // Normalized comparable score (for sorting when currency is equal or estimated)
    double normalizedScore() const
    {
        if (currency.contains("Bits", Qt::CaseInsensitive))
        {
            return amount * 0.01; // 100 bits = $1
        }
        else if (currency.contains("Coins", Qt::CaseInsensitive))
        {
            return amount * 0.013; // rough TikTok coin conversion
        }
        else if (currency.contains("Roses", Qt::CaseInsensitive))
        {
            return amount * 0.01;
        }
        return amount;
    }
    
    QString formattedAmount() const
    {
        if (currency == "$" || currency.isEmpty())
            return QString("$%1").arg(QString::number(amount, 'f', 2));
        else if (currency == "€")
            return QString("€%1").arg(QString::number(amount, 'f', 2));
        else if (currency == "Bits")
            return QString("%1 Bits").arg(static_cast<int>(amount));
        else if (currency == "Coins")
            return QString("%1 Coins").arg(static_cast<int>(amount));
        else if (currency == "Roses")
            return QString("%1 Roses").arg(static_cast<int>(amount));
        else
            return QString("%1 %2").arg(QString::number(amount, 'f', 2), currency);
    }
};
