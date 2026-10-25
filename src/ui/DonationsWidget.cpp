#include "DonationsWidget.h"
#include "DonationManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>

DonationsWidget::DonationsWidget(QWidget* parent)
    : QWidget(parent)
    , m_table(new QTableWidget(this))
    , m_cmbSort(new QComboBox(this))
    , m_btnClear(new QPushButton("Limpiar", this))
    , m_lblTotalCount(new QLabel("Total: 0", this))
{
    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setInterval(250);
    connect(&m_refreshTimer, &QTimer::timeout, this, &DonationsWidget::refreshDonations);
    setupUi();
    refreshDonations();

    connect(&DonationManager::instance(), &DonationManager::donationAdded, this, [this](const Donation& don) {
        Q_UNUSED(don);
        if (!m_refreshTimer.isActive()) m_refreshTimer.start();
    });
    connect(&DonationManager::instance(), &DonationManager::donationsCleared, this, &DonationsWidget::refreshDonations);
}

void DonationsWidget::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // Header & Controls
    auto* topLayout = new QHBoxLayout();
    auto* titleIcon = new QLabel(this);
    titleIcon->setPixmap(QIcon(":/icons/trophy.svg").pixmap(20, 20));
    auto* titleLabel = new QLabel("TOP DONACIONES Y MONETIZACIÓN", this);
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #FFFFFF;");
    topLayout->addWidget(titleIcon);
    topLayout->addWidget(titleLabel);
    topLayout->addStretch();

    auto* lblSort = new QLabel("Ordenar por:", this);
    lblSort->setStyleSheet("color: #9CA3AF; font-size: 12px; font-weight: bold;");
    topLayout->addWidget(lblSort);

    m_cmbSort->addItem("Mayor cantidad", static_cast<int>(DonationSortOrder::HIGHEST_AMOUNT));
    m_cmbSort->addItem("Más recientes", static_cast<int>(DonationSortOrder::MOST_RECENT));
    m_cmbSort->addItem("Plataforma", static_cast<int>(DonationSortOrder::PLATFORM));
    m_cmbSort->addItem("Usuario", static_cast<int>(DonationSortOrder::USERNAME));
    m_cmbSort->setStyleSheet(
        "QComboBox {"
        "  background-color: #1A1D24;"
        "  border: 1px solid #374151;"
        "  border-radius: 6px;"
        "  padding: 6px 12px;"
        "  color: #FFFFFF;"
        "  font-size: 12px;"
        "}"
    );
    connect(m_cmbSort, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DonationsWidget::onSortChanged);
    topLayout->addWidget(m_cmbSort);

    m_btnClear->setIcon(QIcon(":/icons/trash.svg"));
    m_btnClear->setIconSize(QSize(14, 14));
    m_btnClear->setCursor(Qt::PointingHandCursor);
    m_btnClear->setStyleSheet("QPushButton { background: #374151; color: white; border: none; border-radius: 6px; padding: 6px 12px; font-size: 12px; } QPushButton:hover { background: #4B5563; }");
    connect(m_btnClear, &QPushButton::clicked, this, &DonationsWidget::onClearDonationsClicked);
    topLayout->addWidget(m_btnClear);

    mainLayout->addLayout(topLayout);

    // Table
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels({ "#", "Usuario", "Monto", "Plataforma", "Tipo", "Mensaje" });
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setStyleSheet(
        "QTableWidget {"
        "  background-color: #12141A;"
        "  alternate-background-color: #1A1D24;"
        "  border: 1px solid #2D3748;"
        "  border-radius: 8px;"
        "  color: #F3F4F6;"
        "  gridline-color: #2D3748;"
        "  font-size: 12px;"
        "}"
        "QHeaderView::section {"
        "  background-color: #1F2937;"
        "  color: #9CA3AF;"
        "  font-weight: bold;"
        "  border: none;"
        "  padding: 8px;"
        "}"
    );

    mainLayout->addWidget(m_table, 1);

    // Footer
    m_lblTotalCount->setStyleSheet("color: #9CA3AF; font-size: 12px;");
    mainLayout->addWidget(m_lblTotalCount);
}

void DonationsWidget::refreshDonations()
{
    DonationSortOrder sortOrder = static_cast<DonationSortOrder>(m_cmbSort->currentData().toInt());
    QList<Donation> list = DonationManager::instance().getDonations(sortOrder);

    m_table->setRowCount(list.size());
    for (int i = 0; i < list.size(); ++i)
    {
        const auto& don = list[i];

        auto* itemRank = new QTableWidgetItem(QString::number(i + 1));
        itemRank->setTextAlignment(Qt::AlignCenter);

        QString name = don.displayName.isEmpty() ? don.username : don.displayName;
        auto* itemUser = new QTableWidgetItem(name);
        itemUser->setForeground(QBrush(QColor("#60A5FA")));

        auto* itemAmount = new QTableWidgetItem(don.formattedAmount());
        itemAmount->setForeground(QBrush(QColor("#FBBF24")));
        itemAmount->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        auto* itemPlatform = new QTableWidgetItem(don.platform);
        if (don.platform == "Twitch") {
            itemPlatform->setForeground(QBrush(QColor("#A78BFA")));
            itemPlatform->setIcon(QIcon(":/icons/twitch.svg"));
        } else if (don.platform == "YouTube") {
            itemPlatform->setForeground(QBrush(QColor("#F87171")));
            itemPlatform->setIcon(QIcon(":/icons/youtube.svg"));
        } else if (don.platform == "Kick") {
            itemPlatform->setForeground(QBrush(QColor("#4ADE80")));
            itemPlatform->setIcon(QIcon(":/icons/kick.svg"));
        } else if (don.platform == "TikTok") {
            itemPlatform->setForeground(QBrush(QColor("#FB7185")));
            itemPlatform->setIcon(QIcon(":/icons/tiktok.svg"));
        }

        auto* itemType = new QTableWidgetItem(don.type);
        auto* itemMsg = new QTableWidgetItem(don.message);

        m_table->setItem(i, 0, itemRank);
        m_table->setItem(i, 1, itemUser);
        m_table->setItem(i, 2, itemAmount);
        m_table->setItem(i, 3, itemPlatform);
        m_table->setItem(i, 4, itemType);
        m_table->setItem(i, 5, itemMsg);
    }

    m_lblTotalCount->setText(QString("Total de eventos registrados: %1").arg(list.size()));
}

void DonationsWidget::onSortChanged(int index)
{
    Q_UNUSED(index);
    refreshDonations();
}

void DonationsWidget::onClearDonationsClicked()
{
    DonationManager::instance().clear();
}
