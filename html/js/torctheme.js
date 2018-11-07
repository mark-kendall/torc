/*global torc */

var theme = {

"Navbar":
'<nav class="navbar sticky-top navbar-expand-lg navbar-dark bg-dark torc-navbar">' +
'  <a class="navbar-brand torc-application-name" href="#"><%=torc%></a>' +
'  <button class="navbar-toggler" type="button" data-toggle="collapse" data-target="#navbarSupportedContent" aria-controls="navbarSupportedContent" aria-expanded="false" aria-label="Toggle navigation">' +
'    <span class="navbar-toggler-icon"></span>' +
'  </button>' +
'  <div class="collapse navbar-collapse" id="navbarSupportedContent">' +
'    <ul class="navbar-nav ml-auto">' +
'    </ul>' +
'  </div>' +
'</nav>',

"NavbarDropdown":
'<li class="nav-item dropdown <%=ddclass%>">' +
'  <a class="nav-link" href="#" id="navbarDropdown" role="button" data-toggle="dropdown" aria-haspopup="true" aria-expanded="false">' +
'    <i class="fa fa-<%=icon%> fa-lg"></i>' +
'  </a>' +
'  <div class="dropdown-menu dropdown-menu-right <%=menuclass%>" aria-labelledby="navbarDropdown"></div>' +
'</li>',

"NavbarDropdownDivider":
'<div class="dropdown-divider <%=id%>"></div>',

"NavbarDropdownItem":
'<a class="dropdown-item <%=id%>" href="<%=link%>"><%=text%></a>',

"DropdownItemWithIcon":
'<i class="fa fa-<%=icon%>"></i>&nbsp;<%=text%>',

"StategraphContainer":
'<div id="torc-central"></div>',

"StategraphNoConnection":
'<div class="text-center"><i class="fa fa-5x fa-exclamation-circle"></i></div><div class="text-center"><%=text%></div>',

"FileModal":
'<div id="<%=id%>" class="modal fade torcmodal" role="dialog">' +
'  <div class="modal-dialog modal-lg">' +
'    <div class="modal-content">' +
'      <div class="modal-header">' +
'        <h4 class="modal-title"><%=title%></h4>' +
'        <button type="button" class="close" data-dismiss="modal" aria-label="' + torc.CloseTr + '">' +
'          <span aria-hidden="true">&times;</span>' +
'        </button>' +
'      </div>' +
'      <div class="modal-body">' +
'        <pre id="<%=contentid%>"></pre>' +
'      </div>' +
'      <div class="modal-footer">' +
'        <button type="button" class="btn btn-default" data-dismiss="modal">' + torc.CloseTr + '</button>' +
'      </div>' +
'    </div>' +
'  </div>' +
'</div>',

"APIModalID":
"torc-api-modal",

"APIModalContentID":
"torc-api-modal-content",

"APIModal":
'<div id="torc-api-modal" class="modal fade torcmodal" role="dialog">' +
'  <div class="modal-dialog modal-lg">' +
'    <div class="modal-content">' +
'      <div class="modal-header">' +
'        <h4 class="modal-title"><%=title%></h4>' +
'        <button type="button" class="close" data-dismiss="modal" aria-label="' + torc.CloseTr + '">' +
'          <span aria-hidden="true">&times;</span>' +
'        </button>' +
'      </div>' +
'      <div class="modal-body torc-api-modal-content"></div>' +
'      <div class="modal-footer">' +
'        <button type="button" class="btn btn-default" data-dismiss="modal">' + torc.CloseTr + '</button>' +
'      </div>' +
'    </div>' +
'  </div>' +
'</div>',

"APICollapseShow":
'<i class="fa fa-chevron-down"></i>',

"APICollapseHide":
'<i class="fa fa-chevron-up"></i>',

"APIServiceDetail":
'torc-api-service-detail-',

"APIServiceList":
'  <ul class="nav nav-tabs" role="tablist">' +
'    <li class="nav-item"><a class="nav-link active show" role="tab" id="torc-avail-services-tab" data-toggle="tab" aria-controls="torc-avail-services" href="#torc-avail-services">' + torc.ServicesTr + '</a></li>' +
'    <li class="nav-item"><a class="nav-link" role="tab" id="torc-return-formats-tab" data-toggle="tab" aria-controls="torc-return-formats" href="#torc-return-formats">' + torc.ReturnformatsTr + '</a></li>' +
'    <li class="nav-item"><a class="nav-link" role="tab" id="torc-websoc-formats-tab" data-toggle="tab" aria-controls="torc-websoc-formats" href="#torc-websoc-formats">' + torc.WebSocketsTr + '</a></li>' +
'  </ul>' +
'<div class="tab-content">' +

'<div class="tab-pane fade active show" role="tabpanel" aria-labelledby="torc-avail-services-tab" id="torc-avail-services">' +
'  <div class="card" >' +
'  <div class="card-body">' +
'    <div class="card-title">' + torc.AvailableservicesTr + '</div>' +
'    <table class="table table-striped table-hover table-condensed">' +
'      <thead>' +
'        <tr>' +
'          <th>' + torc.IDTr + '</th>' +
'          <th>' + torc.NameTr + '</th>' +
'          <th>' + torc.PathTr + '</th>' +
'          <th>' + torc.DetailsTr + '</th>' +
'        </tr>' +
'      </thead>' +
'      <tbody id="service-accordion">' +
'<% Object.getOwnPropertyNames(services).forEach( function(prop) { %>' +
'        <tr id="api-detail2-<%=prop%>">' +
'          <td><%=prop%></td>' +
'          <td><%=services[prop].name%></td>' +
'          <td><%=services[prop].path%></td>' +
'          <td><button type="button" class="btn btn-primary btn-xs" data-toggle="collapse" aria-controls="api-detail-<%=prop%>" data-target="#api-detail-<%=prop%>"><i class="fa fa-chevron-down"></i></button></td>' +
'        </tr>' +
'        <tr id="api-detail-<%=prop%>" class="collapse" data-parent="#service-accordion" aria-labelledby="api-detail2-<%=prop%>">' +
'          <td colspan="4">' +
'                <div id="torc-api-service-detail-<%=prop%>"</div>' +
'          </td>' +
'        </tr>' +
'<% });%>' +
'      </tbody>' +
'    </table>' +
'  </div>' +
'</div></div>' +

'<div class="tab-pane fade" role="tabpanel" aria-labelledby="torc-return-formats-tab" id="torc-return-formats">' +
'  <div class="card" >' +
'  <div class="card-body">' +
'    <div class="card-title">'+ torc.HTTPreturnformatsTr + '</div>' +
'    <table class="table table-striped table-hover table-condensed">' +
'      <thead>' +
'        <tr>' +
'          <th>' + torc.NameTr + '</th>' +
'          <th>' + torc.ContenttypeTr + '</th>' +
'        </tr>' +
'      </thead>' +
'      <tbody>' +
'<% for (var i = 0; i < formats.length; i++) { %>' +
'        <tr>' +
'          <td><%=formats[i].name%></td>' +
'          <td><%=formats[i].type%></td>' +
'        </tr>' +
'<% };%>' +
'      </tbody>' +
'    </table>' +
'  </div>' +
'</div></div>' +

'<div class="tab-pane fade" role="tabpanel" aria-labelledby="torc-websoc-formats-tab" id="torc-websoc-formats">' +
'  <div class="card" >' +
'  <div class="card-body">' +
'    <div class="card-title">' + torc.WSsubprotocolsTr + '</div>' +
'    <table class="table table-striped table-hover table-condensed">' +
'      <thead>' +
'        <tr>' +
'          <th>' + torc.NameTr + '</th>' +
'          <th>' + torc.DescriptionTr + '</th>' +
'        </tr>' +
'      </thead>' +
'      <tbody>' +
'<% for (var i = 0; i < subprotocols.length; i++) { %>' +
'        <tr>' +
'          <td><%=subprotocols[i].name%></td>' +
'          <td><%=subprotocols[i].description%></td>' +
'        </tr>' +
'<% };%>' +
'      </tbody>' +
'    </table>' +
'  </div>' +
'</div>' +

'</div>',

"APIServiceMethods":
'<div class="panel panel-default">' +
'  <div class="panel-heading">' + torc.MethodlistTr + '</div>' +
'  <div class="panel-body">' +
'    <table class="table table-striped table-hover table-condensed">' +
'      <thead>' +
'        <tr>' +
'          <th></th>' +
'          <th>' + torc.ParametersTr + '</th>' +
'          <th>' + torc.JSreturntypeTr + '</th>' +
'        </tr>' +
'      </thead>' +
'      <tbody>' +
'      <% Object.getOwnPropertyNames(methods).forEach( function(method) { %>' +
'        <tr>' +
'          <td><%=method%></td>' +
'          <td><%=methods[method].params.join()%></td>' +
'          <td><%=methods[method].returns%></td>' +
'        </tr>' +
'      <% });%>' +
'      </body>' +
'    </table>' +
'  </div>' +
'</div>' +
'<div class="panel panel-default">' +
'  <div class="panel-heading">Property list</div>' +
'  <div class="panel-body">' +
'    <table class="table table-striped table-hover table-condensed">' +
'      <thead>' +
'        <tr>' +
'          <th></th>' +
'          <th>' + torc.GetterTr + '</th>' +
'          <th>' + torc.NotificationTr + '</th>' +
'        </tr>' +
'      </thead>' +
'      <tbody>' +
'      <% Object.getOwnPropertyNames(properties).forEach( function(property) { %>' +
'        <tr>' +
'          <td><%=property%></td>' +
'          <td><%=properties[property].read%></td>' +
'          <td><%=properties[property].notification%></td>' +
'        </tr>' +
'      <% });%>' +
'      </body>' +
'    </table>' +
'  </div>' +
'</div>',

"SettingsModalID":
"torc-settings-modal",

"SettingsModalContentID":
"torc-settings-modal-content",

"SettingsModal":
'<div id="torc-settings-modal" class="modal fade torcmodal" role="dialog">' +
'  <div class="modal-dialog modal-lg">' +
'    <div class="modal-content">' +
'      <div class="modal-header">' +
'        <h4 class="modal-title"><%=title%></h4>' +
'        <button type="button" class="close" data-dismiss="modal" aria-label="' + torc.CloseTr + '">' +
'          <span aria-hidden="true">&times;</span>' +
'        </button>' +
'      </div>' +
'      <div class="modal-body torc-settings-modal-content"></div>' +
'      <div class="modal-footer">' +
'        <button type="button" class="btn btn-default" data-dismiss="modal">' + torc.CloseTr + '</button>' +
'      </div>' +
'    </div>' +
'  </div>' +
'</div>',

"SettingsCredentialsButtonId":
'torc-settings-credentials-button',
"SettingsCredentialsFormID":
'torc-settings-credentials-form',
"SettingsCredentialsConfirmID":
'torc-settings-credentials-confirm',
"SettingsUsername1":
'torc-settings-username1',
"SettingsUsername2":
'torc-settings-username2',
"SettingsPassword1":
'torc-settings-password1',
"SettingsPassword2":
'torc-settings-password2',

"SettingsCredentialsButton":
'<div class="card">' +
'  <div class="card-body">' +
'    <div class="card-title">' + torc.ChangeCredsTr + '</div>' +
'    <a id="torc-settings-credentials-button" href="#" class="btn btn-primary">' + torc.UpdateTr + '</a>' +
'<div id="torc-settings-credentials-form" class="collapse">' +
'<div class="card bg-light border-warning mb-3"><div class="card-body"><p class="card-text">' + torc.CredentialsHelpTr + '</p></div></div>' +
'<div class="card"><div class="card-body">' +
'<form>' +
'  <div class="form-group row">' +
'    <label for="torc-settings-username1" class="col-sm-3 col-form-label">' + torc.UsernameTr + '</label>' +
'    <div class="col-sm-9">' +
'      <input type="text" class="form-control" id="torc-settings-username1" />' +
'    </div>' +
'  </div>' +
'  <div class="form-group row">' +
'    <label for="torc-settings-username2" class="col-sm-3 col-form-label">' + torc.Username2Tr + '</label>' +
'    <div class="col-sm-9">' +
'      <input type="text" class="form-control" id="torc-settings-username2" />' +
'    </div>' +
'  </div>' +
'  <div class="form-group row">' +
'    <label for="torc-settings-password1" class="col-sm-3 col-form-label">' + torc.PasswordTr + '</label>' +
'    <div class="col-sm-9">' +
'      <input type="password" class="form-control" id="torc-settings-password1" />' +
'    </div>' +
'  </div>' +
'  <div class="form-group row">' +
'    <label for="torc-settings-password2" class="col-sm-3 col-form-label">' + torc.Password2Tr + '</label>' +
'    <div class="col-sm-9">' +
'      <input type="password" class="form-control" id="torc-settings-password2" />' +
'    </div>' +
'  </div>' +
'</form>' +
'</div><div class="card-footer"><a id="torc-settings-credentials-confirm" href="#" class="btn btn-primary btn-danger">' + torc.ConfirmTr + '</a></div>' +
'</div></div>' +
'  </div>' +
'</div>',

"SettingsListHelpText":
'<small class="form-text text-muted"><%=Help%></small>',

"SettingsListSelect":
'<select class="form-control" id="<%=ID%>">' +
'  <% Object.getOwnPropertyNames(Selections).forEach(function (selection) {%>' +
'    <option value="<%=selection%>"><%=Selections[selection]%></option><%});%>' +
'</select>',

"SettingsList":
'<div class="card">' +
'  <div class="card-body">' +
'    <% function recursive (setting) {' +
'         if (setting.hasOwnProperty("uiname") && setting.hasOwnProperty("name") &&' +
'             setting.hasOwnProperty("type")) { ' +
'           var id  = "torc-settings-" + setting.name;' +
'           var bid = "torc-settings-button-" + setting.name;' +
'           var cid = "torc-settings-button-col-" + setting.name; %>' +
'           <li class="list-group-item"><%' +
'           if (setting.type === "bool")    {%><div class="form-check"><input class="form-check-input" type="checkbox" id="<%=id%>"><label class="form-check-label" for="<%=id%>"><%=setting.uiname%></label><div class="collapse" id="<%=cid%>"><button type="button" class="btn btn-danger" id="<%=bid%>">' + torc.ConfirmTr +'</button></div></div><%}' +
'           if (setting.type === "string")  {%><form onsubmit="return false"><div class="form-group"><label for="<%=id%>"><%=setting.uiname%></label><input class="form-control" type="text" id="<%=id%>"><div class="collapse" id="<%=cid%>"><button type="button" class="btn btn-danger" id="<%=bid%>">' + torc.ConfirmTr +'</button></div></div></form><%}' +
'           if (setting.type === "integer") {%><form onsubmit="return false"><div class="form-group"><label for="<%=id%>"><%=setting.uiname%></label><input class="form-control" type="number" pattern="\d+" id="<%=id%>"><div class="collapse" id="<%=cid%>"><button type="button" class="btn btn-danger" id="<%=bid%>">' + torc.ConfirmTr +'</button></div></div></form><%}' +
'           if (setting.type === "group")   {%><h5><%=setting.uiname%></h5><%}' +
'           if (setting.hasOwnProperty("children") && !$.isEmptyObject(setting.children)) {%>' +
'             <ul class="list-group"><%' +
'               Object.getOwnPropertyNames(setting.children).forEach(function (child) { recursive(setting.children[child]); }); %></ul><%' +
'           }%></li><%' +
'         }' +
'       }; recursive(Settings); %>' +
'  </div>' +
'</div>'
};

if (Object.freeze) { Object.freeze(theme); }
