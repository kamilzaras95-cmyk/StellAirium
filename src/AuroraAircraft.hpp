/*
 * AuroraAircraft — plugin Stellarium pokazujący samoloty na żywo.
 * Copyright (C) 2026 astronow.pl / Kamil Zaras
 *
 * Licensed under GNU GPL v2 or later (Stellarium-compatible).
 */

#ifndef AURORAAIRCRAFT_HPP
#define AURORAAIRCRAFT_HPP

#include "StelModule.hpp"
#include "StelLocation.hpp"

#include <QList>
#include <QObject>
#include <QString>

class StelCore;
class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

//! Pojedyncza migawka samolotu — to co przetrwa parse z adsb.fi.
//! `lat/lon/altM` to pozycja z fetchu (snapshot). `currentLat/Lon/AltM`
//! to pozycja propagowana co klatkę w update() — to ona idzie do toAltAz().
struct AircraftSnapshot
{
	QString icao24;
	QString callsign;
	QString aircraftType;     // ICAO type designator (B738, A320, C152…)

	// Snapshot z fetchu:
	double  lat;              // stopnie
	double  lon;              // stopnie
	double  altM;             // metry n.p.m. (barometric)
	double  speedMs;          // m/s (gs)
	double  trueTrackDeg;     // 0–360°, 0=N, rośnie CW
	double  verticalRateMs;   // m/s, +up
	double  seenPosSec;       // ile sekund temu była ostatnia pozycja (od data.now)

	// Dead-reckoning state — propagowane co klatkę w update(deltaTime):
	double  currentLat;
	double  currentLon;
	double  currentAltM;

	// Wyliczone AltAz względem obserwatora (recompute co klatkę):
	double  altDeg;           // elewacja
	double  azDeg;            // azymut [0–360°)
};

//! Główna klasa pluginu — fetch ADS-B z adsb.fi co 15s, parse, render w draw().
class AuroraAircraft : public StelModule
{
	Q_OBJECT
public:
	AuroraAircraft();
	~AuroraAircraft() override;

	void init() override;
	void update(double deltaTime) override;
	void draw(StelCore* core) override;
	double getCallOrder(StelModuleActionName actionName) const override;

private slots:
	//! Triggerowane przez QTimer co 15s — robi GET na /api/v2/lat/lon/dist.
	void fetchAircraft();
	//! Odpowiedź z adsb.fi — parsuje JSON, aktualizuje aircraftCount + lastStatus.
	void onReply(QNetworkReply* reply);
	//! Stellarium zmienił lokalizację (user kliknął F6) — od razu fetch.
	void onLocationChanged(const StelLocation& loc);

private:
	QNetworkAccessManager* networkMgr;
	QTimer* fetchTimer;

	//! Promień zapytania w nm. 250 ≈ ~460 km — pokrywa horyzont z dużym zapasem.
	int distNm;

	//! Status do wyświetlenia w draw() — zmienia się gdy fetch wraca.
	QString lastStatus;
	int aircraftCount;
	int aboveHorizonCount;

	//! Aktualna lista samolotów po ostatnim fetchu — z policzonym AltAz.
	QList<AircraftSnapshot> aircraft;
};


#include "StelPluginInterface.hpp"

class AuroraAircraftStelPluginInterface : public QObject, public StelPluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID StelPluginInterface_iid)
	Q_INTERFACES(StelPluginInterface)
public:
	StelModule* getStelModule() const override;
	StelPluginInfo getPluginInfo() const override;
	QObjectList getExtensionList() const override { return QObjectList(); }
};

#endif // AURORAAIRCRAFT_HPP
