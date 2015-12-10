﻿<%
var session = require("session");
require("./top.jss");
%>
<div class="caption">REQUEST</div>
<pre><% echo(htmlentities(vardump(request))); %></pre>
<div class="caption">SESSION</div>
<pre>ID: <% echo(htmlentities(session.id)); %></pre>
<pre>INTVAL: <%
if (typeof session.intValue === 'undefined')
    echo("Undefined");
else
    echo(htmlentities(session.intValue.toString()));
%></pre>
<%
//session.RegenerateId();
require("./bottom.jss");
%>
