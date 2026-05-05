/*
 * AuroraAircraft — plugin Stellarium pokazujący samoloty na żywo.
 * Copyright (C) 2026 astronow.pl / Kamil Zaras
 *
 * Licensed under GNU GPL v2 or later (Stellarium-compatible).
 */

#ifndef AURORAAIRCRAFT_HPP
#define AURORAAIRCRAFT_HPP

#include "StelObjectModule.hpp"
#include "StelLocation.hpp"
#include "StelTextureTypes.hpp"

#include <QList>
#include <QObject>
#include <QPointer>
#include <QSettings>
#include <QSharedPointer>
#include <QString>

class AuroraAircraftDialog;

class StelCore;
class QNetworkAccessManager;
class QNetworkReply;
class QTimer;
class AircraftObject;
typedef QSharedPointer<AircraftObject> AircraftObjectP;

//! Pojedyncza migawka samolotu — to co przetrwa parse z adsb.fi.
//! `lat/lon/altM` to pozycja z fetchu (snapshot). `currentLat/Lon/AltM`
//! to pozycja propagowana co klatkę w update() — to ona idzie do toAltAz().
struct AircraftSnapshot
{
	QString icao24;
	QString callsign;
	QString aircraftType;     // ICAO type designator (B738, A320, C152…)
	QString category;         // ADS-B emitter category (A1–A7, B1–B6…)

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
//! Dziedziczymy po StelObjectModule (a nie StelModule) żeby Stellarium mogło
//! traktować samoloty jako selectable objecty (klik → info-panel).
class AuroraAircraft : public StelObjectModule
{
	Q_OBJECT
public:
	AuroraAircraft();
	~AuroraAircraft() override;

	// === StelModule API ===
	void init() override;
	void deinit() override;
	void update(double deltaTime) override;
	void draw(StelCore* core) override;
	double getCallOrder(StelModuleActionName actionName) const override;
	bool configureGui(bool show = true) override;

	// === StelObjectModule API ===
	QList<StelObjectP> searchAround(const Vec3d& v, double limitFov, const StelCore* core) const override;
	StelObjectP searchByNameI18n(const QString& nameI18n) const override;
	StelObjectP searchByName(const QString& name) const override;
	StelObjectP searchByID(const QString& id) const override;
	QVector<QPair<QString, StelObjectP>> listAllObjects(bool inEnglish) const override;
	QString getName() const override { return "Aircraft"; }
	QString getStelObjectType() const override { return "AuroraAircraft"; }

private slots:
	//! Właściwa inicjalizacja uruchamiana po powrocie do event loop.
	void finishInit();
	//! Triggerowane przez QTimer co fetchIntervalSec — robi GET na URL źródła.
	void fetchAircraft();
	//! Odpowiedź z serwera ADS-B — parsuje JSON, aktualizuje aircraftCount + lastStatus.
	void onReply(QNetworkReply* reply);
	//! Stellarium zmienił lokalizację (user kliknął F6) — od razu fetch.
	void onLocationChanged(const StelLocation& loc);
	//! Zapisuje bieżące ustawienia do QSettings.
	void saveSettings() const;

private:
	//! Próbuj podpiąć Stellarium managers, ale nie zakładaj, że są gotowe już w init().
	void ensureRuntimeWiring();

	QNetworkAccessManager* networkMgr;
	QTimer* fetchTimer;

	//! Promień zapytania w nm (domyślnie 250, zmieniany przez dialog).
	int distNm;
	//! Szablon URL źródła danych — %1=lat, %2=lon, %3=distNm.
	QString sourceUrlTemplate;
	//! Interwał fetchu w sekundach.
	int fetchIntervalSec;

	//! Status do wyświetlenia w draw() — zmienia się gdy fetch wraca.
	QString lastStatus;
	int aircraftCount;
	int aboveHorizonCount;
	qint64 latestRequestId;
	QPointer<QNetworkReply> inFlightReply;

	//! Aktualna lista samolotów po ostatnim fetchu — każdy jako StelObject.
	QList<AircraftObjectP> aircraft;

	//! Tekstura sylwetki samolotu, generowana w init() przez QPainter.
	StelTextureSP iconTex;

	AuroraAircraftDialog* configDialog;
	bool initCompleted;
	bool deinitRequested;
	bool objectMgrRegistered;
	bool coreConnected;
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
