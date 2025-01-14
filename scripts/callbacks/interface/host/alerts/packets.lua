--
-- (C) 2019 - ntop.org
--

local alerts_api = require("alerts_api")
local user_scripts = require("user_scripts")

local script = {
  key = "packets",
  local_only = true,

  hooks = {
    all = alerts_api.threshold_check_function
  },

  gui = {
    i18n_title = "alerts_thresholds_config.alert_packets_title",
    i18n_description = "alerts_thresholds_config.alert_packets_description",
    i18n_field_unit = user_scripts.field_units.packets,
    input_builder = user_scripts.threshold_cross_input_builder,
  }
}

-- #################################################################

function script.get_threshold_value(granularity, info)
  local num_packets = host.getPackets()

  return alerts_api.host_delta_val(script.key, granularity, num_packets["packets.sent"] + num_packets["packets.rcvd"])
end

-- #################################################################

return script
