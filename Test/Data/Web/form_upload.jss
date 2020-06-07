<%
var session = require("session");
var topHtml = require("./top.jss");
var bottomHtml = require("./bottom.jss");

topHtml.render();
%>
<div class="row">
	<div class="col">
		<div class="card border-primary mb-3">
			<div class="card-header bg-primary text-white">Form Upload</div>
			<form action="index.jss" method="post" enctype="multipart/form-data">
				<div class="card-body">
					<div class="form-group row">
						<label for="first#	Ã±ame" class="col-sm-2 col-form-label text-right">Test field:</label>
						<div class="col-sm-10">
							<input type="text" class="form-control" name="first#	Ã±ame" />
						</div>
					</div>

					<div class="form-group row">
						<label for="file1" class="col-sm-2 col-form-label text-right">File 1:</label>
						<div class="col-sm-10">
							<input type="file" class="form-control" name="file1" />
						</div>
					</div>

					<div class="form-group row">
						<label class="col-sm-2 col-form-label text-right">File 2[]:</label>
						<div class="col-sm-5">
							<input type="file" class="form-control" name="file2[]" />
						</div>
						<div class="col-sm-5">
							<input type="file" class="form-control" name="file2[]" />
						</div>
					</div>

					<div class="form-group row">
						<label for="file2[aaa]" class="col-sm-2 col-form-label text-right">Test field 2[aaa]:</label>
						<div class="col-sm-10">
							<input type="text" class="form-control" name="file2[aaa]" />
						</div>
					</div>

					<div class="form-group row">
						<label for="file2[bbb]" class="col-sm-2 col-form-label text-right">File 2[bbb]:</label>
						<div class="col-sm-10">
							<input type="file" class="form-control" name="file2[bbb]" />
						</div>
					</div>

					<div class="form-group row">
						<label for="file3" class="col-sm-2 col-form-label text-right">File 3 (multiple):</label>
						<div class="col-sm-10">
							<input type="file" class="form-control" name="file3" multiple />
						</div>
					</div>

					<div class="form-group row">
						<label class="col-sm-2 col-form-label text-right">File 4[0][]:</label>
						<div class="col-sm-5">
							<input type="file" class="form-control" name="file4[0][]" />
						</div>
						<div class="col-sm-5">
							<input type="file" class="form-control" name="file4[0][1]" />
						</div>
					</div>
				</div>
				<div class="card-footer text-primary text-right">
					<button type="submit" class="btn btn-primary">Send</button>
				</div>
			</form>
		</div>
	</div>
</div>
<%
bottomHtml.render();
%>
