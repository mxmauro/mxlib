<%
SetUnhandledExceptionHandler(function(err) {
	resetOutput();
	if (err instanceof SQLiteError) {
		s = err.message;
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

var sqlite = new SQLite();
sqlite.connect(getExecutablePath() + 'mxlib_test.sqlite');

sqlite.query('CREATE TABLE IF NOT EXISTS `test_table` (\r\n' +
                 'id INTEGER PRIMARY KEY AUTOINCREMENT, ' +
                 'varcharField VARCHAR(100), ' +
                 'intField INTEGER, ' +
                 'doubleField DOUBLE, ' +
                 'numericField NUMERIC(5,2), ' +
                 'dateField DATE, ' +
                 'timeField TIME, ' +
                 'timestampField TIMESTAMP, ' +
                 'datetimeField DATETIME, ' +
                 'charField CHAR(100), ' +
                 'blobField BLOB, ' +
                 'textField TEXT ' +
             ');');

sqlite.query("INSERT INTO `test_table` VALUES (" +
                 "NULL, '" + sqlite.escapeString("va\\char data") + "', 100, ?, 256.8, " +
                 "'2015-11-03', '03:45:56', '2015-11-03 03:45:56', ?, " +
                 "'char dat\\a', X'426C6F622044617461', ?" +
             ");", 123.4, new Date(Date.UTC(2015, 11, 3, 3, 45, 56)), "text data");
%>
<pre>Last Insert ID: <% echo(htmlentities(sqlite.insertId.toString())); %></pre>
<%
var row = sqlite.queryAndFetchRow('SELECT * FROM `test_table`;');
while (row !== null) {
%>
<pre>Row #<%= row.id %>: <%= vardump(row) %></pre>
<%
	row = sqlite.fetchRow();
}

bottomHtml.render();
%>
