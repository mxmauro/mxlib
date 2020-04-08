<%
if (request.path.indexOf("/bottom.jss", request.path.length - "/bottom.jss".length) !== -1)
	die("Invalid access");

function render() {
%>
			</div>
		</div>
	</div>
</div>
</body>
</html>
<%
}

module.exports = {
	render
};
%>
