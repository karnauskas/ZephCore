#pragma once

inline const char *rssiQuality(double dbm)
{
	return dbm >= -70 ? "excellent" : dbm >= -100 ? "good" :
		dbm >= -110 ? "fair" : dbm >= -120 ? "weak" : "poor";
}

inline const char *snrQuality(double db)
{
	return db >= 10 ? "excellent" : db >= 0 ? "good" :
		db >= -7.5 ? "fair" : db >= -15 ? "weak" : "poor";
}
