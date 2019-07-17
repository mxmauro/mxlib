<%
if (request.path.indexOf("/bottom.jss", request.path.length - "/bottom.jss".length) !== -1)
	die("Invalid access");

function render() {
%>
</body>
</html>
<%
}

module.exports = {
	render
};
%>
