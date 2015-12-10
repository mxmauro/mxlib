<%
if (request.path.indexOf("/top.jss", request.path.length - "/top.jss".length) !== -1)
	die("Invalid access");
%>
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Demo</title>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<meta name="robots" content="index, follow">
<meta http-equiv="Content-Type" content="text/html;charset=utf-8">
<meta http-equiv="X-UA-Compatible" content="IE=edge">
<link href="index.css" rel="stylesheet" media="screen">
</head>
<body>
<header>
	<div class="header">
		<div class="text">
			<a href="index.jss"><img src="images/header_text.png"></a>
		</div>
	</div>
	<div class="headerShadow"></div>
</header>
<div class="top_menu">
	<a href="form_upload.jss">Form Upload</a>&nbsp;|&nbsp;<a href="increment_cookie_val.jss">Increment cookie value</a>&nbsp;|&nbsp;<a href="mysql_test.jss">MySQL test</a>
</div>
