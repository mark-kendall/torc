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
'    <li class="nav-item"><a class="nav-link active show" role="tab" id="torc-avail-services-tab" data-toggle="tab" aria-controls="torc-avail-services" href="#torc-avail-services">Services</a></li>' +
'    <li class="nav-item"><a class="nav-link" role="tab" id="torc-return-formats-tab" data-toggle="tab" aria-controls="torc-return-formats" href="#torc-return-formats">Return formats</a></li>' +
'    <li class="nav-item"><a class="nav-link" role="tab" id="torc-websoc-formats-tab" data-toggle="tab" aria-controls="torc-websoc-formats" href="#torc-websoc-formats">WebSockets</a></li>' +
'  </ul>' +
'<div class="tab-content">' +

'<div class="tab-pane fade active show" role="tabpanel" aria-labelledby="torc-avail-services-tab" id="torc-avail-services">' +
'  <div class="card" >' +
'  <div class="card-body">' +
'    <div class="card-title">Available services</div>' +
'    <table class="table table-striped table-hover table-condensed">' +
'      <thead>' +
'        <tr>' +
'          <th>ID</th>' +
'          <th>Name</th>' +
'          <th>Path</th>' +
'          <th>Details</th>' +
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
'    <div class="card-title">Supported HTTP return formats</div>' +
'    <table class="table table-striped table-hover table-condensed">' +
'      <thead>' +
'        <tr>' +
'          <th>Name</th>' +
'          <th>Content type</th>' +
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
'    <div class="card-title">Supported WebSocket subprotocols</div>' +
'    <table class="table table-striped table-hover table-condensed">' +
'      <thead>' +
'        <tr>' +
'          <th>Name</th>' +
'          <th>Description</th>' +
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
'  <div class="panel-heading">Method list</div>' +
'  <div class="panel-body">' +
'    <table class="table table-striped table-hover table-condensed">' +
'      <thead>' +
'        <tr>' +
'          <th></th>' +
'          <th>Parameters</th>' +
'          <th>Javascript return type</th>' +
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
'          <th>Getter</th>' +
'          <th>Notification</th>' +
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
'</div>'
};

if (Object.freeze) { Object.freeze(theme); }
