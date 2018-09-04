<%
SetUnhandledExceptionHandler(function(err) {
	resetOutput();
	if (err instanceof MySqlError)
	{
		s = err.message;
		s = s + " [" + err.dbError.toString() + "]";
		die(s);
	}
	else if (err instanceof WindowsError)
	{
		die(err.message);
	}
	else
	{
		die(err.message);
	}
});

var mysql = new MySQL();

mysql.connect('localhost', 'root', 'blaster');

require("./top.jss");

try
{
	mysql.query('CREATE DATABASE `mxlib_test` CHARACTER SET=UTF8;');
}
catch (err)
{
	if (err.dbError != 1007)
		throw err;
}
mysql.selectDatabase('mxlib_test');

try
{
	mysql.query('CREATE TABLE `test_table` (\r\n' +
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
}
catch (err)
{
	if (err.dbError != 1050)
		throw err;
}

mysql.query('INSERT INTO `test_table` VALUES (' +
              'DEFAULT, "' + mysql.escapeString("va\\char data") + '", 100, ?, 256.8, ' +
              '"2015-11-03", "03:45:56",  "2015-11-03 03:45:56", ?, ' +
              '"char dat\\a", "bl\\ob data", ?' +
            ');', 123.4, new Date(Date.UTC(2015, 11, 3, 3, 45, 56)), "text data");


var lastInsertId = mysql.insertId;
%>
<pre>Last Insert ID: <% echo(htmlentities(lastInsertId.toString())); %></pre>
<%
mysql.query('SELECT * FROM `test_table`;');
row = mysql.fetchRow();
while (row !== null)
{
%><pre>Row #<%= row.id %>: <%= vardump(row) %></pre>
<%
	row = mysql.fetchRow();
}
require("./bottom.jss");
%>
