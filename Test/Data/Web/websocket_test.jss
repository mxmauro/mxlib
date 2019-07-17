<%
var topHtml = require("./top.jss");
var bottomHtml = require("./bottom.jss");

topHtml.render();
%>
<p>WebSocket Log</p>
<hr />
<div>
	<textarea id="message" cols="80" rows="5"></textarea>
</div>
<div>
	<input id="sendAsText" type="button" value="Send text" />
	&nbsp;&nbsp;&nbsp;
	<input id="sendAsBlob" type="button" value="Send as binary" />
</div>
<hr />
<div id="log"></div>
<script type="text/javascript">
window.onload = function() {
	initWebSocket();
};

function initWebSocket()
{
	var divLog, elem, url, sck;

	divLog = document.getElementById("log");

	url = (window.location.protocol == 'https:' ? "wss" : "ws") + "://" + window.location.host + "/websocket";
	sck = new WebSocket(url, [ "test" ]);
	sck.binaryType = "arraybuffer";
	sck.onopen = function (e) {
		addToLog(divLog, "Connection opened", '#000080');
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

			addToLog(divLog, "Received: " + s, '#000000');
		}
	};
	sck.onclose = function (e) {
		addToLog(divLog, "Connection closed! (Code: " + e.code.toString() + ")", '#000080');
		sck = null;
	};
	sck.onerror = function (e) {
		addToLog(divLog, "Error in connection", '#C00000');
	};

	elem = document.getElementById("sendAsText");
	elem.onclick = function(e) {
		if (sck && sck.readyState == 1) {
			var textareaElem = document.getElementById("message");
			if (textareaElem.value.length > 0) {
				sck.send(textareaElem.value);
			}
		}
	}

	elem = document.getElementById("sendAsBlob");
	elem.onclick = function(e) {
		if (sck && sck.readyState == 1) {
			var textareaElem, codes, i, ch, val, blob;

			textareaElem = document.getElementById("message");
			codes = [];
			i = 0;
			while (i < textareaElem.value.length) {
				ch = textareaElem.value.charCodeAt(i++);

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
					alert('Invalid hexadecimal input');
					return;
				}
				val = val * 16;

				ch = (i < textareaElem.value.length) ? textareaElem.value.charCodeAt(i++) : 0;
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
					alert('Invalid hexadecimal input');
					return;
				}
				codes.push(val);
			}

			if (codes.length > 0) {
				blob = new Uint8Array(codes.length);
				for (i = 0; i < codes.length; i++)
					blob[i] = codes[i];

				sck.send(blob);
			}
		}
	}
}

function addToLog(parent, text, color)
{
	var p_node = document.createElement("p");
	if (color) {
		p_node.style.color = color;
	}
	p_node.style.marginTop = '0';
	p_node.style.marginBottom = '0';
	var text_node = document.createTextNode(text);
	p_node.appendChild(text_node);
	parent.appendChild(p_node);
}
</script>
<%
bottomHtml.render();
%>
