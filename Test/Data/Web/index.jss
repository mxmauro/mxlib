<%
var session = require("session");
var moment = require("./moment.min.js");
require("./top.jss");
%>
<div class="caption">REQUEST</div>
<pre><%= vardump(request) %></pre>
<div class="caption">SESSION</div>
<pre>ID: <%= session.id %></pre>
<pre>INTVAL: <%
if (typeof session.intValue === 'undefined')
    echo("Undefined");
else
    echo(htmlentities(session.intValue.toString()));
%></pre>
<div class="caption">MOMENT.JS</div>
<pre>ID: <%= moment().toISOString() %></pre>
<%
//session.RegenerateId();
require("./bottom.jss");
%>
