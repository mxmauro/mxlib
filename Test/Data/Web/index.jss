<%
var session = require("session");
var moment = require("./moment.min.js");
require("./top.jss");
%>
<div class="caption">REQUEST</div>
<pre><%= Duktape.enc('jc', request, null, 4) %></pre>
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

<div class="caption">REGEX</div>
<pre>R: <%
	var __s = Duktape.enc('jc', request);
		__s = __s.replace(/[\\"']/g, '\\$&');
		__s = __s.replace(/\u0000/g, '\\0');
	echo( __s );
	%></pre>
<%
//session.RegenerateId();
require("./bottom.jss");
%>
