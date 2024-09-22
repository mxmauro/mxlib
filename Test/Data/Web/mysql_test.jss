<%
SetUnhandledExceptionHandler(function(err) {
	resetOutput();
	if (err instanceof MySqlError) {
		var s = err.message;
		s = s + " [" + err.dbError.toString() + "]";
		die(s);
	}
	else if (err instanceof WindowsError) {
		die(err.message);
	}
	else {
		die(err.message);
	}
});

var topHtml = require("./top.jss");
var bottomHtml = require("./bottom.jss");

topHtml.render();
%>
<div class="row">
	<div class="col">
<%
var mysql = new MySQL();
mysql.connect('localhost', 'root', 'blaster');
mysql.query('CREATE DATABASE IF NOT EXISTS `mxlib_test` CHARACTER SET=UTF8;');
mysql.selectDatabase('mxlib_test');

mysql.query('CREATE TABLE IF NOT EXISTS `test_table` (\r\n' +
                'id INT AUTO_INCREMENT, ' +
                'varcharField VARCHAR(100), ' +
                'intField INT, ' +
                'doubleField DOUBLE, ' +
                'numericField NUMERIC(5,2), ' +
                'dateField DATE, ' +
                'timeField TIME, ' +
                'timestampField TIMESTAMP, ' +
                'datetimeField DATETIME, ' +
                'charField CHAR(100), ' +
                'blobField BLOB, ' +
                'textField TEXT, ' +
                'PRIMARY KEY (id) ' +
            ');');

mysql.query("INSERT INTO `test_table` VALUES (" +
                "DEFAULT, '" + mysql.escapeString("va\\char data") + "', 100, ?, 256.8, " +
                "'2015-11-03', '03:45:56', '2015-11-03 03:45:56', ?, " +
                "'char dat\\a', X'426C6F622044617461', ?" +
            ");", 123.4, new Date(Date.UTC(2015, 11, 3, 3, 45, 56)), "text data");
%>
		<pre>Last Insert ID: <% echo(htmlentities(mysql.insertId.toString())); %></pre>
<%
var row = mysql.queryAndFetchRow('SELECT * FROM `test_table`;');
while (row !== null) {
%>
		<div class="card border-primary mb-3">
			<div class="card-header bg-primary text-white">Row #<%= row.id %></div>
			<div class="card-body text-primary">
				<pre><%= vardump(row) %></pre>
			</div>
		</div>
<%
	row = mysql.fetchRow();
}
%>
	</div>
</div>
<%
bottomHtml.render();
%>
