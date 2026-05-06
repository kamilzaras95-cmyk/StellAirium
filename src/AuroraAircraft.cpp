/*
 * AuroraAircraft — plugin Stellarium pokazujący samoloty na żywo.
 * Copyright (C) 2026 astronow.pl / Kamil Zaras
 *
 * Licensed under GNU GPL v2 or later (Stellarium-compatible).
 */

#include "AuroraAircraft.hpp"
#include "AircraftObject.hpp"
#include "AuroraAircraftDialog.hpp"

#include "StelApp.hpp"
#include "StelCore.hpp"
#include "StelPainter.hpp"
#include "StelProjector.hpp"
#include "StelTexture.hpp"
#include "StelTextureMgr.hpp"
#include "StelUtils.hpp"
#include "VecMath.hpp"

#include <QDebug>
#include <QFont>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPainterPath>
#include <QSettings>
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
	info.id              = "StellAirium";
	info.displayedName   = "StellAirium (live ADS-B)";
	info.authors         = "astronow.pl / Kamil Zaras";
	info.contact         = "https://astronow.pl";
	info.description     = "Pokazuje samoloty na żywo na sferze niebieskiej "
	                       "na podstawie danych ADS-B (adsb.fi).";
	info.version         = AURORAAIRCRAFT_PLUGIN_VERSION;
	info.license         = "GPL v2 or later";
	return info;
}


//
// AuroraAircraft — StelModule.
//

static const QString kDefaultSourceUrl =
    "https://opendata.adsb.fi/api/v2/lat/%1/lon/%2/dist/%3";

static StelCore* findStelCore()
{
	return StelApp::getInstance().getCore();
}

#ifndef STELLAIRIUM_DEBUG_STAGE
#define STELLAIRIUM_DEBUG_STAGE 4
#endif

#ifndef STELLAIRIUM_BUILD_MODE
#define STELLAIRIUM_BUILD_MODE "full"
#endif

#ifndef STELLAIRIUM_BUILD_COMMIT
#define STELLAIRIUM_BUILD_COMMIT "local"
#endif

static constexpr int kStellAiriumDebugStage = STELLAIRIUM_DEBUG_STAGE;
static const char* kStellAiriumBuildMode = STELLAIRIUM_BUILD_MODE;
static const char* kStellAiriumBuildCommit = STELLAIRIUM_BUILD_COMMIT;

AuroraAircraft::AuroraAircraft()
	: networkMgr(nullptr)
	, fetchTimer(nullptr)
	, distNm(250)
	, sourceUrlTemplate(kDefaultSourceUrl)
	, fetchIntervalSec(15)
	, lastStatus("waiting for first fetch...")
	, aircraftCount(0)
	, aboveHorizonCount(0)
	, latestRequestId(0)
	, inFlightReply(nullptr)
	, configDialog(nullptr)
	, initCompleted(false)
	, deinitRequested(false)
	, objectMgrRegistered(false)
	, coreConnected(false)
	, initialFetchDone(false)
{
	setObjectName("AuroraAircraft");

	QSettings s("astronow.pl", "AuroraAircraft");
	distNm            = s.value("distNm",            250).toInt();
	sourceUrlTemplate = s.value("sourceUrlTemplate", kDefaultSourceUrl).toString();
	fetchIntervalSec  = s.value("fetchIntervalSec",  15).toInt();
}


namespace {

constexpr double R_EARTH_M = 6'371'000.0;
constexpr double DEG_TO_RAD = M_PI / 180.0;
constexpr double RAD_TO_DEG = 180.0 / M_PI;

//! Typ wizualny samolotu — port z aurora-web src/transit-detector/src/lib/aircraftIcon.tsx.
enum class AcType { Jet, Heavy, Small, Heli, Glider, Drone };

//! Mapping ADS-B emitter category + heurystyka prędkość/wysokość → typ wizualny.
//! Identyczna logika jak `getAircraftType` we webie.
AcType classifyAircraft(const AircraftSnapshot& a)
{
	const QString& cat = a.category;
	if (cat == "A7") return AcType::Heli;
	if (cat == "A5" || cat == "A6") return AcType::Heavy;
	if (cat == "A1" || cat == "B4") return AcType::Small;
	if (cat == "B1") return AcType::Glider;
	if (cat == "B6") return AcType::Drone;
	if (a.speedMs < 50  && a.currentAltM < 2000) return AcType::Heli;
	if (a.speedMs < 80  && a.currentAltM < 5000) return AcType::Small;
	return AcType::Jet;
}

//! Kolor RGB per typ — port z `typeColor` we webie.
Vec3f colorForType(AcType t)
{
	switch (t)
	{
		case AcType::Heli:   return Vec3f(1.0f,  0.6f,  0.0f);  // pomarańcz
		case AcType::Heavy:  return Vec3f(0.8f,  0.53f, 1.0f);  // fiolet
		case AcType::Small:  return Vec3f(0.53f, 1.0f,  0.73f); // mint
		case AcType::Glider: return Vec3f(1.0f,  1.0f,  0.67f); // żółty
		case AcType::Drone:  return Vec3f(1.0f,  0.53f, 0.8f);  // róż
		case AcType::Jet:
		default:             return Vec3f(0.0f,  0.67f, 1.0f);  // niebieski
	}
}

//! Generuje sylwetkę samolotu (jet — wąskokadłubowy, A320/B737-style) jako QImage.
//! Path skopiowany 1:1 z aurora-web aircraftIcon.tsx (case 'jet').
//! Tekstura biała na transparent — kolor finalny ustawiamy przez StelPainter::setColor().
QImage makeJetIcon(int size)
{
	QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
	img.fill(Qt::transparent);

	QPainter p(&img);
	p.setRenderHint(QPainter::Antialiasing, true);
	p.translate(size / 2.0, size / 2.0);
	const double scale = size / 3.4; // path range ±~1.42 + margin
	p.scale(scale, scale);

	QPainterPath path;
	path.moveTo(0, -1.15);
	path.cubicTo(0.2, -1.15, 0.24, -0.75, 0.24, -0.38);
	path.lineTo(1.42, 0.32);
	path.lineTo(0.24, 0.08);
	path.lineTo(0.24, 0.68);
	path.lineTo(0.68, 1.08);
	path.lineTo(0.24, 0.98);
	path.lineTo(0.15, 1.18);
	path.lineTo(-0.15, 1.18);
	path.lineTo(-0.24, 0.98);
	path.lineTo(-0.68, 1.08);
	path.lineTo(-0.24, 0.68);
	path.lineTo(-0.24, 0.08);
	path.lineTo(-1.42, 0.32);
	path.lineTo(-0.24, -0.38);
	path.cubicTo(-0.24, -0.75, -0.2, -1.15, 0, -1.15);
	path.closeSubpath();

	p.setPen(Qt::NoPen);
	p.fillPath(path, Qt::white);
	return img;
}

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
	a.category     = s.value("category").toString().trimmed();
	a.lat          = s.value("lat").toDouble();
	a.lon          = s.value("lon").toDouble();
	a.altM         = s.value("alt_baro").toDouble(0) * 0.3048;       // ft → m
	a.speedMs      = s.value("gs").toDouble(0) * 0.514444;            // kt → m/s
	a.trueTrackDeg = s.value("track").toDouble(0);
	a.seenPosSec   = s.value("seen_pos").toDouble(0);

	// vert_rate/baro_rate/geom_rate — fpm na m/s. Bierzemy pierwsze niezerowe.
	double vrFpm = s.value("vert_rate").toDouble(0);
	if (vrFpm == 0) vrFpm = s.value("baro_rate").toDouble(0);
	if (vrFpm == 0) vrFpm = s.value("geom_rate").toDouble(0);
	a.verticalRateMs = vrFpm * 0.00508; // fpm → m/s

	// Inicjalizacja current = snapshot. Propagacja w update().
	a.currentLat  = a.lat;
	a.currentLon  = a.lon;
	a.currentAltM = a.altM;
	a.altDeg = a.azDeg = 0;
	return a;
}

} // anonymous namespace

AuroraAircraft::~AuroraAircraft()
{
}

void AuroraAircraft::ensureIconTexture()
{
	if (iconTex)
		return;

	const QImage iconImg = makeJetIcon(64);
	iconTex = StelApp::getInstance().getTextureManager().createTexture(iconImg);
}

void AuroraAircraft::ensureConfigDialog()
{
	if (configDialog)
		return;

	configDialog = new AuroraAircraftDialog();
	configDialog->loadValues(distNm, sourceUrlTemplate, fetchIntervalSec);

	connect(configDialog, &AuroraAircraftDialog::distNmChanged, this, [this](int nm) {
		distNm = nm;
		saveSettings();
	});
	connect(configDialog, &AuroraAircraftDialog::sourceUrlChanged, this, [this](const QString& url) {
		sourceUrlTemplate = url;
		saveSettings();
	});
	connect(configDialog, &AuroraAircraftDialog::fetchIntervalChanged, this, [this](int sec) {
		fetchIntervalSec = sec;
		if (fetchTimer)
			fetchTimer->setInterval(sec * 1000);
		saveSettings();
	});
	connect(configDialog, &AuroraAircraftDialog::fetchNowRequested, this, &AuroraAircraft::fetchAircraft);
}

void AuroraAircraft::init()
{
#ifdef STELLAIRIUM_SMOKE_TEST
	qDebug() << "[StellAirium] build signature"
	         << "mode=" << kStellAiriumBuildMode
	         << "stage=" << kStellAiriumDebugStage
	         << "commit=" << kStellAiriumBuildCommit
	         << "version=" << AURORAAIRCRAFT_PLUGIN_VERSION;
	qDebug() << "[StellAirium] smoke init()";
	return;
#else
	qDebug() << "[StellAirium] build signature"
	         << "mode=" << kStellAiriumBuildMode
	         << "stage=" << kStellAiriumDebugStage
	         << "commit=" << kStellAiriumBuildCommit
	         << "version=" << AURORAAIRCRAFT_PLUGIN_VERSION;
	qDebug() << "[StellAirium] init() start stage" << kStellAiriumDebugStage;
	deinitRequested = false;
	QTimer::singleShot(0, this, &AuroraAircraft::finishInit);
#endif
}

void AuroraAircraft::deinit()
{
#ifdef STELLAIRIUM_SMOKE_TEST
	qDebug() << "[StellAirium] smoke deinit()";
	return;
#else
	qDebug() << "[StellAirium] deinit() start";
	deinitRequested = true;
	objectMgrRegistered = false;
	coreConnected = false;
	initialFetchDone = false;
	if (inFlightReply)
		inFlightReply->abort();
	if (fetchTimer)
	{
		fetchTimer->stop();
		fetchTimer->deleteLater();
		fetchTimer = nullptr;
	}
	if (networkMgr)
	{
		networkMgr->deleteLater();
		networkMgr = nullptr;
	}
	if (configDialog)
	{
		configDialog->hide();
		configDialog->deleteLater();
		configDialog = nullptr;
	}
	initCompleted = false;
	qDebug() << "[StellAirium] deinit() done";
#endif
}

void AuroraAircraft::finishInit()
{
	if (initCompleted || deinitRequested)
		return;
	initCompleted = true;

	qDebug() << "[StellAirium] init() — new QNetworkAccessManager";
	networkMgr = new QNetworkAccessManager(this);
	connect(networkMgr, &QNetworkAccessManager::finished,
	        this, &AuroraAircraft::onReply);
	qDebug() << "[StellAirium] init() — network OK";

	qDebug() << "[StellAirium] init() — defer locationChanged wiring";

	qDebug() << "[StellAirium] init() — new QTimer";
	fetchTimer = new QTimer(this);
	connect(fetchTimer, &QTimer::timeout, this, &AuroraAircraft::fetchAircraft);
	fetchTimer->start(fetchIntervalSec * 1000);
	qDebug() << "[StellAirium] init() — timer OK";

	if (kStellAiriumDebugStage >= 2)
	{
		qDebug() << "[StellAirium] init() — schedule first fetch heartbeat";
		QTimer::singleShot(1000, this, &AuroraAircraft::fetchAircraft);
	}
}

void AuroraAircraft::ensureRuntimeWiring()
{
	static bool loggedWaitingForCore = false;
	if (deinitRequested)
		return;

	if (!coreConnected)
	{
		StelCore* core = findStelCore();
		if (core)
		{
			connect(core, &StelCore::locationChanged,
			        this, &AuroraAircraft::onLocationChanged);
			coreConnected = true;
			qDebug() << "[StellAirium] init() — locationChanged OK";
		}
		else if (!loggedWaitingForCore)
		{
			loggedWaitingForCore = true;
			qDebug() << "[StellAirium] waiting for StelCore in ensureRuntimeWiring()";
		}
	}
}

void AuroraAircraft::update(double deltaTime)
{
#ifdef STELLAIRIUM_SMOKE_TEST
	Q_UNUSED(deltaTime);
	return;
#else
	static int updateHeartbeatCount = 0;
	if (kStellAiriumDebugStage < 2)
	{
		Q_UNUSED(deltaTime);
		return;
	}

	if (updateHeartbeatCount < 5)
	{
		++updateHeartbeatCount;
		qDebug() << "[StellAirium] update heartbeat" << updateHeartbeatCount
		         << "deltaTime=" << deltaTime
		         << "coreConnected=" << coreConnected
		         << "initCompleted=" << initCompleted
		         << "aircraft=" << aircraft.size();
	}

	ensureRuntimeWiring();
	if (!initialFetchDone && initCompleted && networkMgr && coreConnected)
	{
		qDebug() << "[StellAirium] update() — first fetch gate opened";
		initialFetchDone = true;
		fetchAircraft();
	}
	if (aircraft.isEmpty() || deltaTime <= 0) return;

	// Pozycja obserwatora ze Stellarium (uaktualniana per-klatkę,
	// żeby przy zmianie lokalizacji F6 reagować od razu).
	StelCore* core = findStelCore();
	if (!core) return;
	const StelLocation& loc = core->getCurrentLocation();
	const double obsLat = loc.getLatitude();
	const double obsLon = loc.getLongitude();
	const double obsAltM = loc.altitude;

	int newAboveHorizon = 0;
	for (AircraftObjectP& obj : aircraft)
	{
		AircraftSnapshot& a = obj->snap;

		// Dead-reckoning: propaguj currentLat/Lon o deltaTime sekund
		// po wektorze prędkości. Port z aurora-web flightVector.ts:25 (propagatePosition).
		const double trackRad = a.trueTrackDeg * (M_PI / 180.0);
		const double distM = a.speedMs * deltaTime;
		const double cosLat = std::cos(a.currentLat * (M_PI / 180.0));
		a.currentLat += (std::cos(trackRad) * distM) / 111320.0;
		if (cosLat > 1e-9)
			a.currentLon += (std::sin(trackRad) * distM) / (111320.0 * cosLat);
		a.currentAltM += a.verticalRateMs * deltaTime;

		// Re-oblicz AltAz dla nowej pozycji.
		toAltAz(a.currentLat, a.currentLon, a.currentAltM,
		        obsLat, obsLon, obsAltM,
		        a.altDeg, a.azDeg);
		if (a.altDeg > 0) ++newAboveHorizon;
	}
	aboveHorizonCount = newAboveHorizon;
#endif
}

double AuroraAircraft::getCallOrder(StelModuleActionName actionName) const
{
	if (actionName == StelModule::ActionDraw)
		return 1000.0;
	return 0;
}

void AuroraAircraft::fetchAircraft()
{
#ifdef STELLAIRIUM_SMOKE_TEST
	return;
#else
	if (kStellAiriumDebugStage < 2)
		return;

	ensureRuntimeWiring();
	if (!networkMgr)
	{
		qDebug() << "[StellAirium] fetch skipped: networkMgr not ready";
		return;
	}
	const qint64 requestId = ++latestRequestId;
	StelCore* core = findStelCore();
	if (!core)
	{
		qDebug() << "[StellAirium] fetch skipped: StelCore not ready";
		return;
	}
	const StelLocation& loc = core->getCurrentLocation();
	const double lat = loc.getLatitude();
	const double lon = loc.getLongitude();

	const QString urlStr = sourceUrlTemplate
	                       .arg(lat, 0, 'f', 4)
	                       .arg(lon, 0, 'f', 4)
	                       .arg(distNm);

	qDebug() << "[AuroraAircraft] fetch" << urlStr;

	if (inFlightReply)
		inFlightReply->abort();

	QNetworkRequest req((QUrl(urlStr)));
	req.setRawHeader("User-Agent",
	                 "AuroraAircraft-Stellarium-Plugin/" AURORAAIRCRAFT_PLUGIN_VERSION " (+https://astronow.pl)");
	req.setRawHeader("Accept", "application/json");
	inFlightReply = networkMgr->get(req);
	inFlightReply->setProperty("requestId", requestId);
#endif
}

void AuroraAircraft::onReply(QNetworkReply* reply)
{
	const qint64 requestId = reply->property("requestId").toLongLong();
	const bool isLatestReply = (requestId == latestRequestId);
	if (reply == inFlightReply)
		inFlightReply = nullptr;

	reply->deleteLater();

	if (!isLatestReply)
	{
		qDebug() << "[AuroraAircraft] ignoring stale reply" << requestId
		         << "(latest is" << latestRequestId << ")";
		return;
	}

	if (reply->error() != QNetworkReply::NoError)
	{
		if (reply->error() == QNetworkReply::OperationCanceledError)
		{
			lastStatus = "fetch canceled";
			return;
		}
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
	StelCore* core = findStelCore();
	if (!core) return;
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
		toAltAz(a.currentLat, a.currentLon, a.currentAltM,
		        obsLat, obsLon, obsAltM, a.altDeg, a.azDeg);
		if (a.altDeg > 0) ++aboveHorizonCount;

		AircraftObjectP obj(new AircraftObject());
		obj->snap = a;
		aircraft.append(obj);
	}

	const double serverNow = root.value("now").toDouble(0);
	lastStatus = QString("fetched %1 aircraft, %2 above horizon (now=%3)")
	             .arg(aircraftCount)
	             .arg(aboveHorizonCount)
	             .arg(QString::number(serverNow, 'f', 1));

	qDebug().noquote() << "[AuroraAircraft]" << lastStatus;

	if (configDialog)
		configDialog->setStatus(aircraftCount, aboveHorizonCount);
}

bool AuroraAircraft::configureGui(bool show)
{
#ifdef STELLAIRIUM_SMOKE_TEST
	Q_UNUSED(show);
	return false;
#else
	if (kStellAiriumDebugStage < 4)
	{
		Q_UNUSED(show);
		return false;
	}

	ensureConfigDialog();
	if (!configDialog) return false;
	if (show) {
		configDialog->show();
		configDialog->raise();
		configDialog->activateWindow();
	} else {
		configDialog->hide();
	}
	return true;
#endif
}

void AuroraAircraft::saveSettings() const
{
	QSettings s("astronow.pl", "AuroraAircraft");
	s.setValue("distNm",            distNm);
	s.setValue("sourceUrlTemplate", sourceUrlTemplate);
	s.setValue("fetchIntervalSec",  fetchIntervalSec);
}

void AuroraAircraft::onLocationChanged(const StelLocation& loc)
{
#ifdef STELLAIRIUM_SMOKE_TEST
	Q_UNUSED(loc);
	return;
#else
	if (kStellAiriumDebugStage < 2)
	{
		Q_UNUSED(loc);
		return;
	}

	qDebug() << "[AuroraAircraft] location changed → "
	         << loc.getLatitude() << loc.getLongitude()
	         << "— refetching";
	fetchAircraft();
#endif
}

void AuroraAircraft::draw(StelCore* core)
{
#ifdef STELLAIRIUM_SMOKE_TEST
	Q_UNUSED(core);
	return;
#else
	static int drawHeartbeatCount = 0;
	if (kStellAiriumDebugStage < 3)
	{
		Q_UNUSED(core);
		return;
	}

	ensureRuntimeWiring();

	// === 1. Subtelny status w prawym dolnym rogu — niech nie zasłania nieba ===
	StelPainter p2d(core->getProjection2d());
	p2d.setColor(0.4f, 0.85f, 1.0f, 0.55f); // niebieski, pół-przezroczysty
	QFont fontStatus = QGuiApplication::font();
	fontStatus.setPixelSize(11);
	p2d.setFont(fontStatus);

	const float vw = static_cast<float>(p2d.getProjector()->getViewportWidth());
	if (drawHeartbeatCount < 5)
	{
		++drawHeartbeatCount;
		qDebug() << "[StellAirium] draw heartbeat" << drawHeartbeatCount
		         << "aircraft=" << aircraft.size()
		         << "status=" << lastStatus;
	}

	const QString badge = QString("✈ StellAirium — %1 | %2 aloft / %3 in radius")
	                      .arg(lastStatus)
	                      .arg(aboveHorizonCount)
	                      .arg(aircraftCount);
	p2d.drawText(vw - 280.0f, 18.0f, badge);

	if (kStellAiriumDebugStage < 4)
		return;

	if (aircraft.isEmpty())
		return;

	// === 2. Markery samolotow na sferze niebieskiej (AltAz frame) ===
	const StelProjectorP prj = core->getProjection(StelCore::FrameAltAz);
	StelPainter pSky(prj);
	QFont fontLabel = QGuiApplication::font();
	fontLabel.setPixelSize(13);
	pSky.setFont(fontLabel);

	if (kStellAiriumDebugStage < 5)
		return;

	bool useTexture = false;
	bool drawSprite = false;
	bool drawLabels = false;

	if (kStellAiriumDebugStage >= 5)
		drawLabels = true;
	if (kStellAiriumDebugStage >= 6)
		useTexture = true;
	if (kStellAiriumDebugStage >= 7)
		drawSprite = true;

	if (useTexture)
		ensureIconTexture();

	// Tekstura sylwetki + blending — wymagane dla drawSprite2dMode.
	if (useTexture && iconTex)
		iconTex->bind();
	pSky.setBlending(true, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	for (const AircraftObjectP& obj : aircraft)
	{
		const AircraftSnapshot& a = obj->snap;
		if (a.altDeg <= 0) continue; // pod horyzontem — nie ma sensu

		// Stellarium FrameAltAz: +x=south, +y=east, +z=zenith.
		// Nasze az to konwencja geodezyjna 0=N CW. Stellarium "lng" w spheToRect
		// liczone od +x (south) CCW: lng = (180 - az_geodesic) mod 360.
		Vec3d pos;
		const double lng = (180.0 - a.azDeg) * (M_PI / 180.0);
		const double lat = a.altDeg * (M_PI / 180.0);
		StelUtils::spheToRect(lng, lat, pos);

		// Projekcja na ekran. Jeśli punkt nie wpada w viewport — pomiń.
		Vec3d winPos;
		if (!prj->project(pos, winPos)) continue;

		const AcType type = classifyAircraft(a);
		const Vec3f col = colorForType(type);

		// Rozmiar sprite — heavy nieco większy, mały samolot mniejszy.
		float spriteSize = 14.0f;
		if (type == AcType::Heavy)  spriteSize = 18.0f;
		if (type == AcType::Small)  spriteSize = 11.0f;
		if (type == AcType::Glider) spriteSize = 12.0f;
		if (type == AcType::Heli)   spriteSize = 12.0f;
		if (type == AcType::Drone)  spriteSize = 9.0f;

		// Sylwetka: ustaw kolor + rotacja po kursie. Tekstura w QImage jest
		// rysowana z nosem do góry (-Y w QPainter = top of image), a Stellarium
		// drawSprite2dMode rotuje od "nos w prawo" CCW. Eksperymentalnie:
		// rotacja = trueTrackDeg - 90 — żeby track=0 (północ) wskazywał w "górę" ekranu.
		pSky.setColor(col[0], col[1], col[2], 0.95f);
		if (drawSprite)
		{
			pSky.drawSprite2dMode(static_cast<float>(winPos[0]),
			                      static_cast<float>(winPos[1]),
			                      spriteSize,
			                      static_cast<float>(a.trueTrackDeg - 90.0));
		}

		// Etykieta callsign + ICAO type, obok ikonki (nie używamy noGravity bo
		// chcemy żeby label szedł z orientacją widoku).
		if (drawLabels)
		{
			const QString labelMain = a.callsign.isEmpty() ? a.icao24 : a.callsign;
			const QString labelType = a.aircraftType.isEmpty() ? "" : QStringLiteral(" ") + a.aircraftType;
			pSky.drawText(static_cast<float>(winPos[0]) + spriteSize + 2.0f,
			              static_cast<float>(winPos[1]) - 4.0f,
			              labelMain + labelType);
		}
	}
#endif
}


//
// === StelObjectModule API — żeby Stellarium mogło klikać samoloty ===
//

QList<StelObjectP> AuroraAircraft::searchAround(const Vec3d& v, double limitFov,
                                                const StelCore* core) const
{
	QList<StelObjectP> result;
	if (aircraft.isEmpty()) return result;

	const double cosLimitFov = std::cos(limitFov * (M_PI / 180.0));

	// Ważne: render używa core->getProjection(FrameAltAz). Hit-test dostaje `v`
	// w J2000. Żeby porównać w tym samym frame'ie co rysuje, konwertuję klik
	// J2000 → AltAz przez Stellarium-native j2000ToAltAz, i tu liczę dot product
	// z wektorem altAz konstruowanym lokalnie z (alt, az).
	Vec3d vAltAz = core->j2000ToAltAz(v, StelCore::RefractionOff);
	vAltAz.normalize();

	for (const AircraftObjectP& obj : aircraft)
	{
		const AircraftSnapshot& a = obj->snap;
		if (a.altDeg <= 0) continue;

		Vec3d altAzVec;
		const double lng = (180.0 - a.azDeg) * (M_PI / 180.0);
		const double lat = a.altDeg * (M_PI / 180.0);
		StelUtils::spheToRect(lng, lat, altAzVec);

		if (vAltAz.dot(altAzVec) >= cosLimitFov)
			result.append(qSharedPointerCast<StelObject>(obj));
	}
	return result;
}

StelObjectP AuroraAircraft::searchByNameI18n(const QString& nameI18n) const
{
	return searchByName(nameI18n);
}

StelObjectP AuroraAircraft::searchByName(const QString& name) const
{
	const QString needle = name.trimmed().toUpper();
	for (const AircraftObjectP& obj : aircraft)
	{
		if (obj->getEnglishName().toUpper() == needle ||
		    obj->snap.icao24.toUpper() == needle)
			return qSharedPointerCast<StelObject>(obj);
	}
	return StelObjectP();
}

StelObjectP AuroraAircraft::searchByID(const QString& id) const
{
	for (const AircraftObjectP& obj : aircraft)
	{
		if (obj->snap.icao24 == id.toLower())
			return qSharedPointerCast<StelObject>(obj);
	}
	return StelObjectP();
}

QVector<QPair<QString, StelObjectP>> AuroraAircraft::listAllObjects(bool inEnglish) const
{
	Q_UNUSED(inEnglish)
	QVector<QPair<QString, StelObjectP>> result;
	result.reserve(aircraft.size());
	for (const AircraftObjectP& obj : aircraft)
		result.append({obj->getEnglishName(), qSharedPointerCast<StelObject>(obj)});
	return result;
}
