#pragma once

#include <QWidget>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include "AppStyleConfig.h"

class GeneralSettingsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GeneralSettingsWidget(QWidget* parent = nullptr);
    ~GeneralSettingsWidget() override = default;

private slots:
    void onSaveClicked();
    void onResetDefaultsClicked();

private:
    void setupUi();
    void loadSettings();

    // Buffer & General
    QSpinBox* m_spnMaxMessages;
    QCheckBox* m_chkAutoScroll;
    QCheckBox* m_chkSounds;

    // Transient Events & Likes Anti-Spam
    QDoubleSpinBox* m_spnTransientDuration;
    QComboBox* m_cmbLikeMode;
    QSpinBox* m_spnLikeCooldown;
    QCheckBox* m_chkShowJoins;
    QCheckBox* m_chkShowShares;
    QCheckBox* m_chkShowFollows;
    QCheckBox* m_chkShowLikes;

    QPushButton* m_btnSave;
    QPushButton* m_btnReset;
};
