<%
var session = require("session");

if (typeof session.intValue === 'undefined')
    session.intValue = 1;
else
    session.intValue++;
setStatus(302)
setHeader("location", "index.jss");
%>
