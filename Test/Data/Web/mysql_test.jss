<%
var mysql = new MySQL();

if (mysql.connect('localhost', 'root', 'blaster') == false)
	die("Cannot connect to database");
require("./top.jss");

var row = mysql.queryAndFetchRow('SELECT COUNT(*) FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME="mx_lib";');
if (row[0] == 0)
{
	mysql.query('CREATE DATABASE `mxlib_test` CHARACTER SET=UTF8;');
	mysql.selectDatabase('mxlib_test');

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
else
{
	mysql.selectDatabase('mxlib_test');
}

mysql.query('INSERT INTO `test_table` VALUES (\r\n' +
			  'DEFAULT, "vachar data", 100, 123.4, 256.8, \r\n' +
			  '"2015-11-03", "03:45:56",  "2015-11-03 03:45:56", "2015-11-03 03:45:56", \r\n' +
			  '"char data", "blob data", "text data"\r\n' +
			');');


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
