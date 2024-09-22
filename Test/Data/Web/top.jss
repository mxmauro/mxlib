<%
if (request.path.indexOf("/top.jss", request.path.length - "/top.jss".length) !== -1)
	die("Invalid access");

function render() {
%>
<!DOCTYPE html>
<html class="h-100">
<head>
<meta charset="utf-8">
<title>Demo</title>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<meta name="robots" content="index, follow">
<meta http-equiv="Content-Type" content="text/html;charset=utf-8">
<meta http-equiv="X-UA-Compatible" content="IE=edge">
<link href="css/bootstrap.min.css" rel="stylesheet" media="screen">
<link href="css/index.css" rel="stylesheet" media="screen">
<script type="text/javascript" src="js/jquery-3.4.1.min.js"></script>
<script type="text/javascript" src="js/bootstrap.min.js"></script>
</head>
<body class="d-flex flex-column h-100">
<header>
	<div class="header">
		<div class="text">
			<a href="index.jss"><img src="images/header_text.png"></a>
		</div>
	</div>
</header>
<div class="container-fluid flex-grow-1 d-flex">
	<div class="row flex-fill flex-column flex-sm-row">
		<div class="col-sm-1 sidebar flex-shrink-1 bg-dark text-white py-3">
			<nav class="nav flex-sm-column">
				<a class="nav-link custom-nav-link" href="form_upload.jss">Form Upload</a>
				<a class="nav-link custom-nav-link" href="json_xhr.jss">JSON Ajax</a>
				<a class="nav-link custom-nav-link" href="increment_cookie_val.jss">Increment cookie value</a>
				<a class="nav-link custom-nav-link" href="mysql_test.jss">MySQL test</a>
				<a class="nav-link custom-nav-link" href="sqlite_test.jss">SQLite test</a>
				<a class="nav-link custom-nav-link" href="websocket_test.jss">WebSocket test</a>
			</nav>
		</div>
		<div class="col-sm-11 col-sm-offset-1 py-2 flex-grow-1">
			<div class="container-fluid">
<%
}

module.exports = {
	render
};
%>
