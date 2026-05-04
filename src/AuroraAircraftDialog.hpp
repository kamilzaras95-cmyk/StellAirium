/*
 * AuroraAircraft — plugin Stellarium pokazujący samoloty na żywo.
 * Copyright (C) 2026 astronow.pl / Kamil Zaras
 * Licensed under GNU GPL v2 or later.
 */

#ifndef AURORAARCRAFTDIALOG_HPP
#define AURORAARCRAFTDIALOG_HPP

#include <QDialog>
#include <QString>

class QLabel;
class QSlider;
class QComboBox;
class QLineEdit;

class AuroraAircraftDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AuroraAircraftDialog(QWidget* parent = nullptr);

    void setStatus(int total, int aboveHorizon);
    //! Ustawia kontrolki bez emitowania sygnałów — wywołaj przy starcie po załadowaniu QSettings.
    void loadValues(int distNm, const QString& sourceUrlTemplate, int intervalSec);

signals:
    void distNmChanged(int nm);
    void sourceUrlChanged(const QString& urlTemplate);
    void fetchIntervalChanged(int seconds);
    void fetchNowRequested();

private slots:
    void onDistSliderChanged(int sliderValue);
    void onSourceChanged(int index);
    void onCustomUrlChanged(const QString& text);
    void onIntervalChanged(int index);

private:
    QLabel*    statusLabel;
    QSlider*   distSlider;
    QLabel*    distValueLabel;
    QComboBox* sourceCombo;
    QLineEdit* customUrlEdit;
    QComboBox* intervalCombo;
};

#endif // AURORAARCRAFTDIALOG_HPP
