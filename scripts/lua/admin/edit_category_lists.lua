--
-- (C) 2018 - ntop.org
--

local dirs = ntop.getDirs()
package.path = dirs.installdir .. "/scripts/lua/modules/?.lua;" .. package.path

require "lua_utils"
require "graph_utils"
local template = require "template_utils"
local categories_utils = require "categories_utils"
local lists_utils = require "lists_utils"
local page_utils = require("page_utils")
local json = require("dkjson")
local format_utils = require("format_utils")
sendHTTPContentTypeHeader('text/html')

if not haveAdminPrivileges() then
  return
end

page_utils.print_header()
active_page = "admin"

dofile(dirs.installdir .. "/scripts/lua/inc/menu.lua")

local base_url = ntop.getHttpPrefix() .. "/lua/admin/edit_category_lists.lua"
local page_params = {
  category = _GET["category"],
}

local lists = lists_utils.getCategoryLists()

if _POST["action"] == "edit" then
  local enabled = not isEmptyString(_POST["list_enabled"])
  local list_name = _POST["list_name"]
  local category = tonumber(split(_POST["category"], "_")[2])
  local url = _POST["url"]
  local list_update = tonumber(_POST["list_update"])

  url = string.gsub(url, "http:__", "http://")
  url = string.gsub(url, "https:__", "https://")

  lists_utils.editList(list_name, {
    enabled = enabled,
    category = category,
    url = url,
    update_interval = list_update,
  })
elseif _POST["action"] == "update" then
  local list_name = _POST["list_name"]
  lists_utils.updateList(list_name)

  print('<div class="alert alert-success alert-dismissable"><a href="" class="close" data-dismiss="alert" aria-label="close">&times;</a>'..
    i18n('category_lists.list_will_be_updated', {name=list_name}) .. '</div>')
end

print[[
  <form id="list-update-form" method="post">
    <input type="hidden" name="csrf" value="]] print(ntop.getRandomCSRFValue()) print[[" />
    <input type="hidden" name="action" value="update" />
    <input id="list_to_update" type="hidden" name="list_name" />
  </form>

  <!-- Modal -->
  <div id="editListModal" class="modal fade in" role="dialog">
    <div class="modal-dialog">

      <!-- Modal content-->
      <div class="modal-content">
        <div class="modal-header">
          <button type="button" class="close" data-dismiss="modal">&times;</button>
          <h3 class="modal-title">]] print(i18n("category_lists.edit_list")) print[[</h3>
        </div>
        <div class="modal-body">
          <div class="container-fluid">
            <form id="edit-list-form" method="post" data-toggle="validator">
              <input type="hidden" name="csrf" value="]] print(ntop.getRandomCSRFValue()) print[[" />
              <input type="hidden" name="action" value="edit" />

              <div class="row form-group has-feedback">
                <div class="col col-md-12">
                  <label class="form-label">]] print(i18n("name")) print[[</label>
                  <input name="list_name" id="form-edit-name" class="form-control" type="text" readonly />
                </div>
              </div>

              <div class="row form-group has-feedback">
                <div class="col col-md-12">
                  <label class="form-label">]] print(i18n("flow_details.url")) print[[</label>
                  <input name="url" class="form-control" type="text" readonly />
                </div>
              </div>

              <div class="row form-group">
                <div class="col col-md-12">
                  <label class="form-label">]] print(i18n("category_lists.enabled")) print[[&nbsp;</label>
                  <input name="list_enabled" type="checkbox" id="form-edit-enable" />
                </div>
              </div>

              <div class="row form-group">
                <div class="col col-md-6">
                  <label class="form-label">]] print(i18n("category")) print[[</label>
                  <select name="category" class="form-control" readonly disabled="disabled">]]

                  for cat_name, cat_id in pairsByKeys(interface.getnDPICategories()) do
                    print(string.format([[<option value="cat_%s">%s</option>]], cat_id, cat_name))
                  end

                  print[[</select>
                </div>
                <div class="col col-md-6">
                  <label class="form-label">]] print(i18n("category_lists.update_frequency")) print[[</label>
                  <select name="list_update" class="form-control">
                    <option value="86400">]] print(i18n("alerts_thresholds_config.daily")) print[[</option>
                    <option value="3600">]] print(i18n("alerts_thresholds_config.hourly")) print[[</option>
                  </select>
                </div>
              </div>

              <br>
              <div class="form-group">
                <button type="submit" class="btn btn-primary btn-block">]] print(i18n("category_lists.edit_list")) print[[</button>
              </div>
            </div>
          </form>
        </div>
      </div>
    </div>
  </div>
]]

print[[<hr><h2>]] print(i18n("category_lists.category_lists")) print[[</h2>]]

print[[
<div id="table-edit-lists-form"></div>]]

local stats = ntop.getCache("ntopng.cache.category_lists.load_stats")
if(stats) then
  stats = json.decode(stats)

  if(stats) then
    print(i18n("category_lists.loading_stats", {
      when = format_utils.formatPastEpochShort(stats.begin),
      num_hosts = stats.num_hosts,
      num_ips = stats.num_ips,
      num_ja3 = stats.num_ja3,
      duration = secondsToTime(stats.duration),
    }))
  end
end

print[[
<script>
  var url_update = "]] print(getPageUrl(ntop.getHttpPrefix()..[[/lua/admin/get_category_lists.lua]], page_params)) print[[";

  $("#table-edit-lists-form").datatable({
    url: url_update,
    class: "table table-striped table-bordered",
    title:"",
    buttons: []]

local categories = {}

for _, list in pairs(lists) do
  local catid = tostring(list.category)
  categories[catid] = categories[catid] or 0
  categories[catid] = categories[catid] + 1
end

  printCategoryDropdownButton(false, page_params.category, base_url, page_params, function (catid, catname)
    return(categories[catid] or 0)
  end)

print[[],
    columns: [
      {
        title: "]] print(i18n("name")) print[[",
        field: "column_label",
        sortable: true,
      }, {
        title: "]] print(i18n("status")) print[[",
        field: "column_status",
        sortable: true,
        css: {
            textAlign: 'center',
        }
      }, {
        title: "]] print(i18n("category")) print[[",
        field: "column_category_name",
        sortable: true,
      }, {
        title: "]] print(i18n("category_lists.last_update")) print[[",
        field: "column_last_update",
        sortable: true,
        css: {
          textAlign: 'center',
        }
      }, {
        title: "]] print(i18n("graphs.metrics_prefixes.num_hosts")) print[[",
        field: "column_num_hosts",
        sortable: true,
        css: {
            textAlign: 'center',
        }
      }, {
        title: "]] print(i18n("actions")) print[[",
        field: "column_actions",
        sortable: false,
          css: {
            textAlign: 'center',
        }
      }, {
        field: "column_category",
        hidden: 1,
      }, {
        field: "column_name",
        hidden: 1,
      }
    ], rowCallback: function(row, data) {
      var list_name = data.column_name;
      var enabled = data.column_enabled;
      var actions_td_idx = 6;

      datatableAddActionButtonCallback.bind(row)(actions_td_idx, "prepareEditListModal('" + list_name + "'); $('#editListModal').modal('show');", "]] print(i18n('users.edit')) print[[");

      if(enabled)
        datatableAddActionButtonCallback.bind(row)(actions_td_idx, "$('#list_to_update').val('" + list_name + "'); $('#list-update-form').submit()", "]] print(i18n('category_lists.update_now')) print[[");

      return row;
     }
  });

  function prepareEditListModal(list_name) {
    var data = datatableGetColumn($("#table-edit-lists-form"), "column_name", list_name);
    var form = $("#edit-list-form");

    form.find("[name='list_name']").val(data.column_name);
    form.find("[name='list_enabled']").prop('checked', data.column_enabled);
    form.find("[name='url']").val(data.column_url);
    form.find("[name='category']").val(data.column_category);
    form.find("[name='list_update']").val(data.column_update_interval);
  }
</script>

<br><br>
]]

dofile(dirs.installdir .. "/scripts/lua/inc/footer.lua")
