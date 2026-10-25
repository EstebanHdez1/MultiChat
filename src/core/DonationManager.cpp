#include "DonationManager.h"
#include "Logger.h"
#include <algorithm>

DonationManager& DonationManager::instance()
{
    static DonationManager s_instance;
    return s_instance;
}

DonationManager::DonationManager(QObject* parent)
    : QObject(parent)
{
}

void DonationManager::addDonation(const Donation& donation)
{
    {
        QMutexLocker locker(&m_mutex);
        m_donations.append(donation);
        while (m_donations.size() > MAX_DONATIONS)
        {
            m_donations.removeFirst();
        }
    }
    
    Logger::instance().info("DonationManager", QString("New donation from [%1] %2: %3 (%4)")
        .arg(donation.platform, donation.displayName.isEmpty() ? donation.username : donation.displayName, 
             donation.formattedAmount(), donation.type));
             
    emit donationAdded(donation);
}

QList<Donation> DonationManager::getDonations(DonationSortOrder sortOrder) const
{
    QMutexLocker locker(&m_mutex);
    QList<Donation> list = m_donations;

    switch (sortOrder)
    {
    case DonationSortOrder::HIGHEST_AMOUNT:
        std::sort(list.begin(), list.end(), [](const Donation& a, const Donation& b) {
            return a.normalizedScore() > b.normalizedScore();
        });
        break;
    case DonationSortOrder::MOST_RECENT:
        std::sort(list.begin(), list.end(), [](const Donation& a, const Donation& b) {
            return a.timestamp > b.timestamp;
        });
        break;
    case DonationSortOrder::PLATFORM:
        std::sort(list.begin(), list.end(), [](const Donation& a, const Donation& b) {
            if (a.platform == b.platform)
                return a.normalizedScore() > b.normalizedScore();
            return a.platform < b.platform;
        });
        break;
    case DonationSortOrder::USERNAME:
        std::sort(list.begin(), list.end(), [](const Donation& a, const Donation& b) {
            QString nameA = a.displayName.isEmpty() ? a.username : a.displayName;
            QString nameB = b.displayName.isEmpty() ? b.username : b.displayName;
            return nameA.compare(nameB, Qt::CaseInsensitive) < 0;
        });
        break;
    }

    return list;
}

void DonationManager::clear()
{
    {
        QMutexLocker locker(&m_mutex);
        m_donations.clear();
    }
    emit donationsCleared();
}

int DonationManager::totalDonationCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_donations.size();
}
