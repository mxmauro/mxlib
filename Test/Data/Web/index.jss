<%
var session = require("session");
var moment = require("./moment.min.js");
var topHtml = require("./top.jss");
var bottomHtml = require("./bottom.jss");

topHtml.render();
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
<pre><% echo(Duktape.enc('jc', request).replace(/[\\"']/g, '\\$&').replace(/\u0000/g, '\\0')); %></pre>
<div class="caption">JWT Token</div>
<%
var token = JWT.create({
	name: 'admin',
	value: 1,
	data: [ 10, 20, 30 ]
}, 'some-password', {
	algorithm: 'HS256',
	issuer: 'mx-library',
	expiresAfter: '10 minutes'
});
%>
<pre>Encoded: <%= token %></pre>
<%
var decoded_token = JWT.verify(token, 'some-password', {
	algorithm: 'HS256'
});
%>
<pre>Decoded: <% echo(Duktape.enc('jc', decoded_token).replace(/[\\"']/g, '\\$&').replace(/\u0000/g, '\\0')); %></pre>
<%
//session.RegenerateId();
bottomHtml.render();
%>
