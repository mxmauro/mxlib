<%
var topHtml = require("./top.jss");
var bottomHtml = require("./bottom.jss");

topHtml.render();
%>
<div class="row">
	<div class="col">
		<div class="card border-primary mb-3">
			<div class="card-header bg-primary text-white">WebSocket</div>
			<div class="card-body">
				<textarea id="message" class="form-control" cols="80" rows="5"></textarea>
				<small id="message_error" style="display:none;" class="text-danger"></small>
			</div>
			<div class="card-footer text-primary text-right">
				<input id="sendAsText" type="button" class="btn btn-primary"value="Send text" />
				&nbsp;&nbsp;&nbsp;
				<input id="sendAsBlob" type="button" class="btn btn-primary"value="Send as binary" />
			</div>
		</div>
		<div id="log"></div>
	</div>
</div>
<script type="text/javascript">
$(document).ready(function() {
	initWebSocket();
});

function initWebSocket() {
	var elem, url, sck;

	url = (window.location.protocol == 'https:' ? "wss" : "ws") + "://" + window.location.host + "/websocket";
	sck = new WebSocket(url, [ "test" ]);
	sck.binaryType = "arraybuffer";
	sck.onopen = function (e) {
		log("Connection opened", '#000080');
		sck.send("Here's some text that the server is urgently awaiting!"); 
	};
	sck.onmessage = function(e) {
		if (sck && sck.readyState == 1) {
			var i, s, view;

			if (e.data instanceof ArrayBuffer) {
				s = "(arraybuffer)";
				view = new Uint8Array(e.data);
				len = (view.length > 32) ? 32 : view.length;
				for (i = 0; i < len; i++) {
					s += " " + ("00" + view[i].toString(16)).slice(-2);
				}
			}
			else {
				s = "(string) " + e.data;
			}

			log("Received: " + s, '#000000');
		}
	};
	sck.onclose = function (e) {
		log("Connection closed! (Code: " + e.code.toString() + ")", '#000080');
		sck = null;
	};
	sck.onerror = function (e) {
		log("Error in connection", '#C00000');
	};

	$("#message").keydown(function () {
		$("#message_error").hide();
	});

	$("#sendAsText").click(function() {
		if (sck && sck.readyState == 1) {
			var msg = $("#message").val();
			if (msg.length > 0) {
				sck.send(msg);
			}
		}
	});

	$("#sendAsBlob").click(function() {
		if (sck && sck.readyState == 1) {
			var msg = $("#message").val();
			var codes = [];
			var i = 0;
			while (i < msg.length) {
				var val;
				var ch = msg.charCodeAt(i++);

				if (ch >= 48 && ch <= 57) {
					val = ch - 48;
				}
				else if (ch >= 65 && ch <= 70) {
					val = ch - 55;
				}
				else if (ch >= 97 && ch <= 102) {
					val = ch - 87;
				}
				else if (ch == 32 || ch == 13 || ch == 10) {
					continue;
				}
				else {
					$("#message_error").text("Invalid hexadecimal input");
					$("#message_error").show();
					return;
				}
				val = val * 16;

				ch = (i < msg.length) ? msg.charCodeAt(i++) : 0;
				if (ch >= 48 && ch <= 57) {
					val += ch - 48;
				}
				else if (ch >= 65 && ch <= 70) {
					val += ch - 55;
				}
				else if (ch >= 97 && ch <= 102) {
					val += ch - 87;
				}
				else {
					$("#message_error").text("Invalid hexadecimal input");
					$("#message_error").show();
					return;
				}
				codes.push(val);
			}

			if (codes.length > 0) {
				var blob = new Uint8Array(codes.length);
				for (i = 0; i < codes.length; i++) {
					blob[i] = codes[i];
				}

				sck.send(blob);
			}
		}
	});
}

function log(text, color)
{
	var p = $('<p></p>');
	if (color) {
		p.css('color', color);
	}
	p.css('marginTop', 0);
	p.css('marginBottom', 0);
	p.text(text);
	$("#log").append(p)
}
</script>
<%
bottomHtml.render();
%>
