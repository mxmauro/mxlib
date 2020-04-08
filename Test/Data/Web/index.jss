<%
var session = require("session");
var moment = require("./moment.min.js");
var topHtml = require("./top.jss");
var bottomHtml = require("./bottom.jss");

topHtml.render();
%>
<div class="row">
	<div class="col">
		<div class="card border-primary mb-3">
			<div class="card-header bg-primary text-white">REQUEST</div>
			<div class="card-body text-primary">
				<pre><%= Duktape.enc('jc', request, null, 4) %></pre>
			</div>
		</div>

		<div class="card border-primary mb-3">
			<div class="card-header bg-primary text-white">SESSION</div>
			<div class="card-body text-primary">
				<pre>ID: <%= session.id %></pre>
				<pre>INTVAL: <%
				if (typeof session.intValue === 'undefined')
					echo("Undefined");
				else
					echo(htmlentities(session.intValue.toString()));
				%></pre>
			</div>
		</div>

		<div class="card border-primary mb-3">
			<div class="card-header bg-primary text-white">MOMENT.JS</div>
			<div class="card-body text-primary">
				<pre>ID: <%= moment().toISOString() %></pre>
			</div>
		</div>

		<div class="card border-primary mb-3">
			<div class="card-header bg-primary text-white">REGEX</div>
			<div class="card-body text-primary">
				<pre><% echo(Duktape.enc('jc', request).replace(/[\\"']/g, '\\$&').replace(/\u0000/g, '\\0')); %></pre>
			</div>
		</div>

		<div class="card border-primary mb-3">
			<div class="card-header bg-primary text-white">JWT TOKEN</div>
			<div class="card-body text-primary">
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
				%><pre>Encoded: <%= token %></pre><%
				var decoded_token = JWT.verify(token, 'some-password', {
					algorithm: 'HS256'
				});
				%><pre>Decoded: <% echo(Duktape.enc('jc', decoded_token).replace(/[\\"']/g, '\\$&').replace(/\u0000/g, '\\0')); %></pre>
			</div>
		</div>
	</div>
</div>
<%
//session.RegenerateId();
bottomHtml.render();
%>
