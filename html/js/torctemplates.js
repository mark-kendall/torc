var MainNavBar =
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
'</div>';

var MainNavBarDropdown =
'<li class="dropdown <%=ddclass%>">' +
'  <a class="dropdown-toggle" href="#" data-toggle="dropdown">' +
'    <i class="fa fa-<%=icon%> fa-lg"></i>' +
'  </a>' +
'  <ul class="dropdown-menu <%=menuclass%>" role="menu"></ul>' +
'</li>';

var MainNavBarDropdownDivider =
'<li class="divider <%=id%>"></li>';

var MainNavBarDropdownItem =
'<li class="<%=id%>">' +
'  <a href="<%=link%>"><%=text%></a>' +
'</li>';

var DropdownItemWithIcon =
'<i class="fa fa-<%=icon%>">&nbsp;</i><%=text%>';

var TorcCentralDiv =
'<div id="torc-central"></div>';

var TorcCentralNoConnection =
'<div class="row text-center"><i class="fa fa-5x fa-exclamation-circle"></i>&nbsp;<%=text%></div>';

var DisplayFileModal =
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
'</div>';

// I don't know, I only work here...

// Simple JavaScript Templating
// John Resig - http://ejohn.org/ - MIT Licensed
(function(){
  var cache = {};

  this.template = function template(str, data){
    // Figure out if we're getting a template, or if we need to
    // load the template - and be sure to cache the result.
    var fn = !/\W/.test(str) ?
      cache[str] = cache[str] ||
        tmpl(document.getElementById(str).innerHTML) :

      // Generate a reusable function that will serve as a template
      // generator (and which will be cached).
      new Function("obj",
        "var p=[],print=function(){p.push.apply(p,arguments);};" +

        // Introduce the data as local variables using with(){}
        "with(obj){p.push('" +

        // Convert the template into pure JavaScript
        str
          .replace(/[\r\t\n]/g, " ")
          .split("<%").join("\t")
          .replace(/((^|%>)[^\t]*)'/g, "$1\r")
          .replace(/\t=(.*?)%>/g, "',$1,'")
          .split("\t").join("');")
          .split("%>").join("p.push('")
          .split("\r").join("\\'")
      + "');}return p.join('');");

    // Provide some basic currying to the user
    return data ? fn( data ) : fn;
  };
})();
