/*
 * AuroraAircraft — plugin Stellarium pokazujący samoloty na żywo.
 * Copyright (C) 2026 astronow.pl / Kamil Zaras
 *
 * Licensed under GNU GPL v2 or later (Stellarium-compatible).
 */

#include "AuroraAircraft.hpp"

#include "StelApp.hpp"
#include "StelCore.hpp"
#include "StelModuleMgr.hpp"
#include "StelPainter.hpp"
#include "StelProjector.hpp"
#include "StelUtils.hpp"
#include "VecMath.hpp"

#include <QDebug>
#include <QFont>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>


//
// StelPluginInterface — wywoływane przez StelModuleMgr po Q_IMPORT_PLUGIN.
//

StelModule* AuroraAircraftStelPluginInterface::getStelModule() const
{
	return new AuroraAircraft();
}

StelPluginInfo AuroraAircraftStelPluginInterface::getPluginInfo() const
{
	StelPluginInfo info;
	info.id              = "AuroraAircraft";
	info.displayedName   = "Aurora Aircraft (live ADS-B)";
	info.authors         = "astronow.pl / Kamil Zaras";
	info.contact         = "https://astronow.pl";
	info.description     = "Pokazuje samoloty na żywo na sferze niebieskiej "
	                       "na podstawie danych ADS-B (adsb.fi). "
	                       "Side-loadowy plugin dla patronów astronow.pl.";
	info.version         = "0.0.2";
	info.license         = "GPL v2 or later";
	return info;
}


//
// AuroraAircraft — StelModule.
//

AuroraAircraft::AuroraAircraft()
	: networkMgr(nullptr)
	, fetchTimer(nullptr)
	, distNm(250)
	, lastStatus("waiting for first fetch...")
	, aircraftCount(0)
	, aboveHorizonCount(0)
{
	setObjectName("AuroraAircraft");
}


namespace {

constexpr double R_EARTH_M = 6'371'000.0;
constexpr double DEG_TO_RAD = M_PI / 180.0;
constexpr double RAD_TO_DEG = 180.0 / M_PI;

//! Konwersja pozycji samolotu (WGS84 lat/lon/alt) → AltAz wzgl. obserwatora.
//! Port 1:1 z aurora-web src/transit-detector/src/lib/flightVector.ts:49.
//! ECEF → ENU (East-North-Up), brak korekcji refrakcji (samoloty są blisko, mało istotne).
void toAltAz(double planeLat, double planeLon, double planeAltM,
             double obsLat,   double obsLon,   double obsAltM,
             double& outAltDeg, double& outAzDeg)
{
	const double cosOLat = std::cos(obsLat * DEG_TO_RAD);
	const double sinOLat = std::sin(obsLat * DEG_TO_RAD);
	const double cosOLon = std::cos(obsLon * DEG_TO_RAD);
	const double sinOLon = std::sin(obsLon * DEG_TO_RAD);

	const double rObs = R_EARTH_M + obsAltM;
	const double Ox = rObs * cosOLat * cosOLon;
	const double Oy = rObs * cosOLat * sinOLon;
	const double Oz = rObs * sinOLat;

	const double rPlane = R_EARTH_M + planeAltM;
	const double Px = rPlane * std::cos(planeLat * DEG_TO_RAD) * std::cos(planeLon * DEG_TO_RAD);
	const double Py = rPlane * std::cos(planeLat * DEG_TO_RAD) * std::sin(planeLon * DEG_TO_RAD);
	const double Pz = rPlane * std::sin(planeLat * DEG_TO_RAD);

	const double dx = Px - Ox;
	const double dy = Py - Oy;
	const double dz = Pz - Oz;

	// Rotacja ECEF → ENU
	const double e = -sinOLon * dx + cosOLon * dy;
	const double n = -sinOLat * cosOLon * dx - sinOLat * sinOLon * dy + cosOLat * dz;
	const double u =  cosOLat * cosOLon * dx + cosOLat * sinOLon * dy + sinOLat * dz;

	const double range = std::sqrt(dx*dx + dy*dy + dz*dz);
	outAltDeg = std::asin(u / range) * RAD_TO_DEG;
	outAzDeg  = std::fmod(std::atan2(e, n) * RAD_TO_DEG + 360.0, 360.0);
}

//! Pojedynczy samolot z payloadu adsb.fi. Zwraca puste std::nullopt-equivalent
//! (icao24 pusty) dla rekordów bez pozycji albo na ziemi.
AircraftSnapshot parseAc(const QJsonObject& s)
{
	AircraftSnapshot a;
	a.icao24 = "";

	if (!s.contains("lat") || !s.contains("lon")) return a;
	if (s.value("alt_baro").toString() == "ground") return a;
	if (s.value("lat").isNull() || s.value("lon").isNull()) return a;

	a.icao24       = s.value("hex").toString().toLower();
	a.callsign     = s.value("flight").toString().trimmed();
	a.aircraftType = s.value("t").toString().trimmed();
	a.lat          = s.value("lat").toDouble();
	a.lon          = s.value("lon").toDouble();
	a.altM         = s.value("alt_baro").toDouble(0) * 0.3048;       // ft → m
	a.speedMs      = s.value("gs").toDouble(0) * 0.514444;            // kt → m/s
	a.trueTrackDeg = s.value("track").toDouble(0);
	a.seenPosSec   = s.value("seen_pos").toDouble(0);
	a.altDeg = a.azDeg = 0;
	return a;
}

} // anonymous namespace

AuroraAircraft::~AuroraAircraft()
{
}

void AuroraAircraft::init()
{
	qDebug() << "[AuroraAircraft] init()";

	networkMgr = new QNetworkAccessManager(this);
	connect(networkMgr, &QNetworkAccessManager::finished,
	        this, &AuroraAircraft::onReply);

	// Reaguj na zmianę lokalizacji w Stellarium (F6) — natychmiast fetch.
	connect(StelApp::getInstance().getCore(), &StelCore::locationChanged,
	        this, &AuroraAircraft::onLocationChanged);

	fetchTimer = new QTimer(this);
	connect(fetchTimer, &QTimer::timeout, this, &AuroraAircraft::fetchAircraft);
	fetchTimer->start(15000);  // 15s — pod limit ToS adsb.fi (~1 req/s) z dużym buforem

	// Pierwszy fetch od razu, bez czekania 15s.
	QTimer::singleShot(500, this, &AuroraAircraft::fetchAircraft);
}

void AuroraAircraft::update(double deltaTime)
{
	Q_UNUSED(deltaTime)
}

double AuroraAircraft::getCallOrder(StelModuleActionName actionName) const
{
	if (actionName == StelModule::ActionDraw)
		return 1000.0;
	return 0;
}

void AuroraAircraft::fetchAircraft()
{
	const StelLocation& loc = StelApp::getInstance().getCore()->getCurrentLocation();
	const double lat = loc.getLatitude();
	const double lon = loc.getLongitude();

	const QString urlStr = QString("https://opendata.adsb.fi/api/v2/lat/%1/lon/%2/dist/%3")
	                       .arg(lat, 0, 'f', 4)
	                       .arg(lon, 0, 'f', 4)
	                       .arg(distNm);

	qDebug() << "[AuroraAircraft] fetch" << urlStr;

	QNetworkRequest req((QUrl(urlStr)));
	req.setRawHeader("User-Agent",
	                 "AuroraAircraft-Stellarium-Plugin/0.0.2 (+https://astronow.pl)");
	req.setRawHeader("Accept", "application/json");
	networkMgr->get(req);
}

void AuroraAircraft::onReply(QNetworkReply* reply)
{
	reply->deleteLater();

	if (reply->error() != QNetworkReply::NoError)
	{
		lastStatus = QString("error: %1").arg(reply->errorString());
		qWarning() << "[AuroraAircraft] fetch failed:" << reply->errorString();
		return;
	}

	const QByteArray body = reply->readAll();
	QJsonParseError parseErr;
	const QJsonDocument doc = QJsonDocument::fromJson(body, &parseErr);
	if (parseErr.error != QJsonParseError::NoError || !doc.isObject())
	{
		lastStatus = QString("bad JSON: %1").arg(parseErr.errorString());
		qWarning() << "[AuroraAircraft] JSON parse error:" << parseErr.errorString();
		return;
	}

	const QJsonObject root = doc.object();
	const QJsonArray ac = root.value("aircraft").toArray();
	aircraftCount = ac.size();

	// Pozycja obserwatora ze Stellarium — używamy tej samej, której user właśnie patrzy.
	StelCore* core = StelApp::getInstance().getCore();
	const StelLocation& loc = core->getCurrentLocation();
	const double obsLat = loc.getLatitude();
	const double obsLon = loc.getLongitude();
	const double obsAltM = loc.altitude;

	aircraft.clear();
	aboveHorizonCount = 0;
	aircraft.reserve(ac.size());

	for (const QJsonValue& v : ac)
	{
		AircraftSnapshot a = parseAc(v.toObject());
		if (a.icao24.isEmpty()) continue;
		toAltAz(a.lat, a.lon, a.altM, obsLat, obsLon, obsAltM, a.altDeg, a.azDeg);
		if (a.altDeg > 0) ++aboveHorizonCount;
		aircraft.append(a);
	}

	const double serverNow = root.value("now").toDouble(0);
	lastStatus = QString("fetched %1 aircraft, %2 above horizon (now=%3)")
	             .arg(aircraftCount)
	             .arg(aboveHorizonCount)
	             .arg(QString::number(serverNow, 'f', 1));

	qDebug().noquote() << "[AuroraAircraft]" << lastStatus;
}

void AuroraAircraft::onLocationChanged(const StelLocation& loc)
{
	qDebug() << "[AuroraAircraft] location changed → "
	         << loc.getLatitude() << loc.getLongitude()
	         << "— refetching";
	fetchAircraft();
}

void AuroraAircraft::draw(StelCore* core)
{
	// === 1. Status (2D, lewy gorny rog) ===
	StelPainter p2d(core->getProjection2d());
	p2d.setColor(0.4f, 1.0f, 0.6f, 1.0f);
	QFont fontStatus = QGuiApplication::font();
	fontStatus.setPixelSize(16);
	fontStatus.setBold(true);
	p2d.setFont(fontStatus);

	const QString l1 = QString("AuroraAircraft v0.0.4 — %1 aircraft (%2 above horizon)")
	                   .arg(aircraftCount).arg(aboveHorizonCount);
	p2d.drawText(40, 80, l1);
	p2d.drawText(40, 105, lastStatus);

	// === 2. Markery samolotow na sferze niebieskiej (AltAz frame) ===
	const StelProjectorP prj = core->getProjection(StelCore::FrameAltAz);
	StelPainter pSky(prj);
	pSky.setColor(0.4f, 0.85f, 1.0f, 0.95f); // jet-blue, jak w naszym webie
	QFont fontLabel = QGuiApplication::font();
	fontLabel.setPixelSize(13);
	pSky.setFont(fontLabel);

	for (const AircraftSnapshot& a : aircraft)
	{
		if (a.altDeg <= 0) continue; // pod horyzontem — nie ma sensu

		// Stellarium FrameAltAz: +x=south, +y=east, +z=zenith.
		// Nasze az to konwencja geodezyjna 0=N CW. Stellarium "lng" w spheToRect
		// liczone od +x (south) CCW: lng = (180 - az_geodesic) mod 360.
		Vec3d pos;
		const double lng = (180.0 - a.azDeg) * (M_PI / 180.0);
		const double lat = a.altDeg * (M_PI / 180.0);
		StelUtils::spheToRect(lng, lat, pos);

		const QString labelMain = a.callsign.isEmpty() ? a.icao24 : a.callsign;
		const QString labelType = a.aircraftType.isEmpty() ? "" : QStringLiteral("  ") + a.aircraftType;

		// "•" jako marker + callsign + ICAO type. xshift/yshift przesuwa label
		// obok markera, true = noGravity (label nie obraca sie z polem widzenia).
		pSky.drawText(pos, QStringLiteral("●  ") + labelMain + labelType, 0, 6, 6, true);
	}
}
