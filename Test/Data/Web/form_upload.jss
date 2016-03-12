<%
var session = require("session");
require("./top.jss");
%>
<form action="index.jss" method="post" enctype="multipart/form-data">
<div>
    <p>Test field: <input type="text" name='first#	Ã±ame' /></p>
    <p>File 1: <input type="file" name='file1' /></p>
    <p>File 2a: <input type="file" name='file2[]' /></p>
    <p>File 2b: <input type="file" name='file2[]' /></p>
    <p>Test field 2c: <input type="text" name='file2[aaa]' /></p>
    <p>File 2d: <input type="file" name='file2[bbb]' /></p>
    <p>File 3 (multiple): <input type="file" name='file3' multiple /></p>
    <p>File 4a: <input type="file" name='file4[0][]' /></p>
    <p>File 4b: <input type="file" name='file4[0][1]' /></p>
    <p><input type="submit" value="Send" /></p>
</div>
</form>
<%
require("./bottom.jss");
%>
