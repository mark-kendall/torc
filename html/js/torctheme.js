var theme = {

"Navbar":
'<div class="navbar navbar-inverse navbar-fixed-top torc-navbar" role="navigation">' +
'  <div class="container">' +
'    <div class="navbar-header">' +
'      <button type="button" class="navbar-toggle" data-toggle="collapse" data-target=".navbar-collapse">' +
'        <span class="icon-bar"></span>' +
'        <span class="icon-bar"></span>' +
'        <span class="icon-bar"></span>' +
'      </button>' +
'      <a class="navbar-brand torc-application-name" href="#"><%=torc%></a>' +
'    </div>' +
'    <div class="collapse navbar-collapse navbar-right">' +
'      <ul class="nav navbar-nav">' +
'      </ul>' +
'    </div>' +
'  </div>' +
'</div>',

"NavbarDropdown":
'<li class="dropdown <%=ddclass%>">' +
'  <a class="dropdown-toggle" href="#" data-toggle="dropdown">' +
'    <i class="fa fa-<%=icon%> fa-lg"></i>' +
'  </a>' +
'  <ul class="dropdown-menu <%=menuclass%>" role="menu"></ul>' +
'</li>',

"NavbarDropdownDivider":
'<li class="divider <%=id%>"></li>',

"NavbarDropdownItem":
'<li class="<%=id%>">' +
'  <a href="<%=link%>"><%=text%></a>' +
'</li>',

"DropdownItemWithIcon":
'<i class="fa fa-<%=icon%>">&nbsp;</i><%=text%>',

"StategraphContainer":
'<div id="torc-central"></div>',

"StategraphNoConnection":
'<div class="row text-center"><i class="fa fa-5x fa-exclamation-circle"></i>&nbsp;<%=text%></div>',

"FileModal":
'<div id="<%=id%>" class="modal fade" role="dialog">' +
'  <div class="modal-dialog modal-lg">' +
'    <div class="modal-content">' +
'      <div class="modal-header">' +
'        <button type="button" class="close" data-dismiss="modal">&times;</button>' +
'        <h4 class="modal-title"><%=title%></h4>' +
'      </div>' +
'      <div class="modal-body">' +
'        <pre id="<%=contentid%>"></pre>' +
'      </div>' +
'      <div class="modal-footer">' +
'        <button type="button" class="btn btn-default" data-dismiss="modal">Close</button>' +
'      </div>' +
'    </div>' +
'  </div>' +
'</div>',

"APIModalID":
"torc-api-modal",

"APIModalContentID":
"torc-api-modal-content",

"APIModal":
'<div id="torc-api-modal" class="modal fade" role="dialog">' +
'  <div class="modal-dialog modal-lg">' +
'    <div class="modal-content">' +
'      <div class="modal-header">' +
'        <button type="button" class="close" data-dismiss="modal">&times;</button>' +
'        <h4 class="modal-title"><%=title%></h4>' +
'      </div>' +
'      <div class="modal-body torc-api-modal-content"></div>' +
'      <div class="modal-footer">' +
'        <button type="button" class="btn btn-default" data-dismiss="modal">Close</button>' +
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
'  <ul class="nav nav-tabs">' +
'    <li class="active"><a data-toggle="tab" href="#torc-available-services">Services</a></li>' +
'    <li><a data-toggle="tab" href="#torc-return-formats">Return formats</a></li>' +
'    <li><a data-toggle="tab" href="#torc-websoc-formats">WebSockets</a></li>' +
'  </ul>' +
'<div class="tab-content">' +

'<div class="panel panel-default tab-pane fade in active" id="torc-available-services">' +
'  <div class="panel-heading" >Available services</div>' +
'  <div class="panel-body">' +
'    <table class="table table-striped table-hover table-condensed">' +
'      <thead>' +
'        <tr>' +
'          <th>ID</th>' +
'          <th>Name</th>' +
'          <th>Path</th>' +
'          <th>Details</th>' +
'        </tr>' +
'      </thead>' +
'      <tbody' +
'<% Object.getOwnPropertyNames(services).forEach( function(prop) { %>' +
'        <tr>' +
'          <td><%=prop%></td>' +
'          <td><%=services[prop].name%></td>' +
'          <td><%=services[prop].path%></td>' +
'          <td><button type="button" class="btn btn-primary btn-xs" data-toggle="collapse" data-target="#api-detail-<%=prop%>"><i class="fa fa-chevron-down"></i></button></td>' +
'        </tr>' +
'        <tr id="api-detail-<%=prop%>" class="collapse collapse-api">' +
'          <td colspan="4">' +
'                <div id="torc-api-service-detail-<%=prop%>"</div>' +
'          </td>' +
'        </tr>' +
'<% });%>' +
'      </tbody>' +
'    </table>' +
'  </div>' +
'</div>' +

'<div class="panel panel-default tab-pane fade" id="torc-return-formats">' +
'  <div class="panel-heading">Supported HTTP return formats</div>' +
'  <div class="panel-body">' +
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
'</div>' +

'<div class="panel panel-default tab-pane fade" id="torc-websoc-formats">' +
'  <div class="panel-heading">Supported WebSocket subprotocols</div>' +
'  <div class="panel-body">' +
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
