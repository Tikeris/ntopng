/*
 *
 * (C) 2019 - ntop.org
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "ntop_includes.h"

/* ****************************************** */

AlertableEntity::AlertableEntity(NetworkInterface *iface, AlertEntity entity) {
  alert_iface = iface;
  entity_type = entity, num_triggered_alerts = 0;

  for(u_int i = 0; i < MAX_NUM_PERIODIC_SCRIPTS; i++)
    locks[i] = NULL;

  suppressed_alerts = false;
  refreshSuppressedAlert();
}

/* ****************************************** */

AlertableEntity::~AlertableEntity() {
  for(u_int i = 0; i < MAX_NUM_PERIODIC_SCRIPTS; i++) {
    if(locks[i])
      delete locks[i];
  }
}

/* ****************************************** */

RwLock* AlertableEntity::getLock(ScriptPeriodicity p) {
  try {
    if(locks[(u_int)p] || (locks[(u_int)p] = new RwLock()))
      return locks[(u_int)p];
  } catch(std::bad_alloc& ba) {
    ntop->getTrace()->traceEvent(TRACE_ERROR, "Memory allocation error");
  }

  return NULL;
}

/* ****************************************** */

void AlertableEntity::rdLock(ScriptPeriodicity p, const char *filename, int line) {
  RwLock *rwl;

  if((rwl = getLock(p)))
    rwl->rdlock(filename, line);
}

/* ****************************************** */

void AlertableEntity::wrLock(ScriptPeriodicity p, const char *filename, int line) {
  RwLock *rwl;

  if((rwl = getLock(p)))
    rwl->wrlock(filename, line);
}

/* ****************************************** */

void AlertableEntity::unlock(ScriptPeriodicity p, const char *filename, int line) {
  RwLock *rwl;

  if((rwl = getLock(p)))
    rwl->unlock(filename, line);
}

/* ****************************************** */

void AlertableEntity::luaAlert(lua_State* vm, const Alert *alert, ScriptPeriodicity p) const {
  /* NOTE: must conform to the AlertsManager format */
  lua_push_int32_table_entry(vm,  "alert_type", alert->alert_type);
  lua_push_str_table_entry(vm,    "alert_subtype", alert->alert_subtype.c_str());
  lua_push_int32_table_entry(vm,  "alert_severity", alert->alert_severity);
  lua_push_int32_table_entry(vm,  "alert_entity", entity_type);
  lua_push_str_table_entry(vm,    "alert_entity_val", entity_val.c_str());
  lua_push_uint64_table_entry(vm, "alert_tstamp", alert->alert_tstamp_start);
  lua_push_uint64_table_entry(vm, "alert_tstamp_end", alert->last_update);
  lua_push_int32_table_entry(vm,  "alert_granularity", Utils::periodicityToSeconds((ScriptPeriodicity)p));
  lua_push_str_table_entry(vm,    "alert_json", alert->alert_json.c_str());
}

/* ****************************************** */

/* Return true if the element was inserted, false if already present.
   NOTE: given a ScriptPeriodicity p, only one thread at time can perform
   a triggerAlert. */
bool AlertableEntity::triggerAlert(lua_State* vm, std::string key,
				   ScriptPeriodicity p, time_t now,
				   AlertLevel alert_severity, AlertType alert_type,
				   const char *alert_subtype,
				   const char *alert_json) {
  bool rv = false;
  std::map<std::string, Alert>::iterator it;

  if(entity_val.empty()) {
    ntop->getTrace()->traceEvent(TRACE_ERROR, "setEntityValue() not called or empty entity_val");
  } else {
    wrLock(p, __FILE__, __LINE__);

    it = triggered_alerts[(u_int)p].find(key);

    if(it == triggered_alerts[(u_int)p].end()) {
      Alert alert;

      alert.alert_tstamp_start = alert.last_update = now;
      alert.alert_severity = alert_severity;
      alert.alert_type = alert_type;
      alert.alert_subtype = alert_subtype;
      alert.alert_json = alert_json;

      alert_iface->incNumAlertsEngaged(p);

      triggered_alerts[(u_int)p][key] = alert;

      lua_newtable(vm);
      luaAlert(vm, &alert, p);

      rv = true; /* Actually inserted */
    }

    unlock(p, __FILE__, __LINE__);
  }

  if(!rv)
    lua_pushnil(vm);
  
  return(rv);
}

/* ****************************************** */

bool AlertableEntity::releaseAlert(lua_State* vm,
				   std::string key, ScriptPeriodicity p, time_t now) {
  std::map<std::string, Alert>::iterator it;
  bool rv = false;

  if(!triggered_alerts[(u_int)p].empty()) {
    wrLock(p, __FILE__, __LINE__);

    it = triggered_alerts[(u_int)p].find(key);

    if(it != triggered_alerts[(u_int)p].end()) {
      /* Set the release time */
      it->second.last_update = now;

      /* Found, push the alert */
      lua_newtable(vm);
      luaAlert(vm, &it->second, p);

      alert_iface->decNumAlertsEngaged(p);

      triggered_alerts[(u_int)p].erase(it);

      rv = true; /* Actually released */
    }

    unlock(p, __FILE__, __LINE__);
  }

  if(!rv)
    lua_pushnil(vm);

  return(rv);
}

/* ****************************************** */

void AlertableEntity::updateNumTriggeredAlerts() {
  int i;
  u_int num_alerts = 0;

  for(i = 0; i<MAX_NUM_PERIODIC_SCRIPTS; i++)
    num_alerts += getNumTriggeredAlerts((ScriptPeriodicity) i);

  num_triggered_alerts = num_alerts;
}

/* ****************************************** */

void AlertableEntity::countAlerts(grouped_alerts_counters *counters) {
  std::map<std::string, Alert>::const_iterator it;

  for(int i = 0; i < MAX_NUM_PERIODIC_SCRIPTS; i++) {
    ScriptPeriodicity p = (ScriptPeriodicity)i;

    if(!triggered_alerts[p].empty()) {
      rdLock(p, __FILE__, __LINE__);

      for(it = triggered_alerts[p].begin(); it != triggered_alerts[p].end(); ++it) {
	const Alert *alert = &it->second;
	
	counters->severities[alert->alert_severity]++;
	counters->types[alert->alert_type]++;
      }

      unlock(p, __FILE__, __LINE__);
    }
  }
}

/* ****************************************** */

void AlertableEntity::getPeriodicityAlerts(lua_State* vm, ScriptPeriodicity p,
				AlertType type_filter, AlertLevel severity_filter, u_int *idx) {
  std::map<std::string, Alert>::const_iterator it;

  if(!triggered_alerts[p].empty()) {
    rdLock(p, __FILE__, __LINE__);

    for(it = triggered_alerts[p].begin(); it != triggered_alerts[p].end(); ++it) {
      const Alert *alert = &it->second;

      if(((type_filter == alert_none)
	  || (type_filter == alert->alert_type))
	 && ((severity_filter == alert_level_none)
	     || (severity_filter == alert->alert_severity))) {
	lua_newtable(vm);
	luaAlert(vm, alert, (ScriptPeriodicity)p);

	lua_pushinteger(vm, ++(*idx));
	lua_insert(vm, -2);
	lua_settable(vm, -3);
      }
    }

    unlock(p, __FILE__, __LINE__);
  }
}

/* ****************************************** */

void AlertableEntity::getAlerts(lua_State* vm, ScriptPeriodicity periodicity_filter,
				AlertType type_filter, AlertLevel severity_filter, u_int *idx) {
  if(periodicity_filter != no_periodicity) {
    /* Get alerts about a specific periodicity */
    getPeriodicityAlerts(vm, periodicity_filter, type_filter, severity_filter, idx);
  } else {
    int p;

    for(p = 0; p < MAX_NUM_PERIODIC_SCRIPTS; p++)
      getPeriodicityAlerts(vm, (ScriptPeriodicity)p, type_filter, severity_filter, idx);
  }
}

/* ****************************************** */

u_int AlertableEntity::getNumTriggeredAlerts(ScriptPeriodicity p) const {
  return triggered_alerts[p].size();
}

/* ****************************************** */

void AlertableEntity::syncReadonlyTriggeredAlerts() {
  updateNumTriggeredAlerts();
}

/* ****************************************** */

void AlertableEntity::refreshSuppressedAlert() {
  if(!entity_val.empty()) {
    if(!ntop->getPrefs()->are_alerts_disabled()) {
      char rsp[64], rkey[128];
      snprintf(rkey, sizeof(rkey), CONST_SUPPRESSED_ALERT_PREFS, alert_iface->get_id());

      if(ntop->getRedis()->hashGet(rkey, entity_val.c_str(), rsp, sizeof(rsp)) == 0)
        suppressed_alerts = ((strcmp(rsp, "false") == 0) ? 1 : 0);
      else
        suppressed_alerts = false;
    } else
      suppressed_alerts = true;
  }
}
