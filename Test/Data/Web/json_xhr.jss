<%
if (request.method == "POST") {
	echo(JSON.stringify(request.json));
	exit();
}
var session = require("session");
var topHtml = require("./top.jss");
var bottomHtml = require("./bottom.jss");

topHtml.render();
%>
<div class="row">
	<div class="col">
		<div class="card border-primary mb-3">
			<div class="card-header bg-primary text-white">JSON Ajax</div>
			<div class="card-body">
				<textarea id="json_text" class="form-control" cols="80" rows="5"></textarea>
			</div>
			<div class="card-footer text-primary text-right">
				<input id="send" type="button" class="btn btn-primary" value="Send" />
			</div>
		</div>
		<div id="result"></div>
	</div>
</div>
<script type="text/javascript">
$(document).ready(function() {
	initJsonAjax();
});

function initJsonAjax()
{
	$("#send").click(function () {
		$.ajax({
			type: "POST",
			url: "json_xhr.jss",
			data: $("#json_text").val(),
			contentType: "application/json; charset=utf-8",
			processData: false,
			dataType: "text"
		}).done(function (data) {
			var s = "Success";
			if (typeof data === "string") {
				s = s + " [" + data + "]";
			}
			$("#result").text(data);
		}).fail(function(jqXHR, textStatus) {
			var s = "Error " + jqXHR.status.toString();
			if (jqXHR.responseText && jqXHR.responseText.length > 0) {
				s = s + ": " + jqXHR.responseText;
			}
			$("#result").text(s);
		});
	});
}
</script>
<%
bottomHtml.render();
%>
