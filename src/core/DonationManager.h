#pragma once

#include <QObject>
#include <QList>
#include <QMutex>
#include "Donation.h"

enum class DonationSortOrder
{
    HIGHEST_AMOUNT,
    MOST_RECENT,
    PLATFORM,
    USERNAME
};

class DonationManager : public QObject
{
    Q_OBJECT

public:
    static DonationManager& instance();

    void addDonation(const Donation& donation);
    QList<Donation> getDonations(DonationSortOrder sortOrder = DonationSortOrder::HIGHEST_AMOUNT) const;
    void clear();

    int totalDonationCount() const;

signals:
    void donationAdded(const Donation& donation);
    void donationsCleared();

private:
    DonationManager(QObject* parent = nullptr);
    ~DonationManager() override = default;

    mutable QMutex m_mutex;
    QList<Donation> m_donations;
    static constexpr int MAX_DONATIONS = 200;
};
