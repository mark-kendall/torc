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
'</div>'
};

if (Object.freeze) { Object.freeze(theme); }
