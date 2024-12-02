#!/bin/bash


scp_dir=$(readlink -f "$0")
scp_dir=$(dirname "$scp_dir")
csv_file="$1"

cat "${scp_dir}/${csv_file}" | awk -F/ 'BEGIN{ print "FuncName = [" }
/bench_text_cat|bench_copy_block/{
	printf("\t\"%s\",\n", substr($2,2));
} END{ print "];" }' | awk '!a[$0]++' | tr _ '-' > "${scp_dir}/memcpy_plot.m"

cat "${scp_dir}/${csv_file}" | awk -F, '/^name/{
	printf("HeadName = [\"%s\", \"function\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"];\n", $1, $2, $3, $4, $5, $6);
	printf("Result = [\n");
} /^"/{
	split($1, name, "([\"/()]|<[0-9xa-fA-F]+>)[\"/()]*");
	printf("\t\"%s/%s/%s/%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\";\n", name[2], name[4], name[5], name[6], name[3], $2, $3, $4, $5, $6);
} END{ print "];" }' | tr _ '-' >> "${scp_dir}/memcpy_plot.m"

cat >> "${scp_dir}/memcpy_plot.m" <<"EOT"

TestCase = unique(Result(:,1),'stable');
TestCaseMean = TestCase(contains(TestCase, "mean"));
TestCaseCv = TestCase(contains(TestCase, "-cv"));
for i = 1:length(TestCaseMean)
	fig = figure;
	A = Result(Result(:,1) == TestCaseMean(i), :);
	B = A(:,2);
	Y = double(A(:,7));
	X = categorical(B);
	X = reordercats(X, B);
	barh(X, Y);
	title(TestCaseMean(i));
	while fig.isvalid
		pause(1);
	end
end

for i = 1:length(TestCaseCv)
	fig = figure;
	A = Result(Result(:,1) == TestCaseCv(i), :);
	X = categorical(A(:,2));
	X = reordercats(X, A(:,2));
	barh(X, double(A(:,7)));
	title(TestCaseCv(i));
	while fig.isvalid
		pause(1);
	end
end

EOT




