--
-- first, define the functions.  Turn off echoing so that expected file
-- does not depend on contents of pgxml.sql.
--
SET client_min_messages = warning;
\set ECHO none
\i pgxml.sql
\set ECHO all
RESET client_min_messages;

select query_to_xml('select 1 as x',true,false,'');

select xslt_process( query_to_xml('select x from generate_series(1,5) as 
x',true,false,'')::text,
$$<xsl:stylesheet version="1.0"
               xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output method="xml" indent="yes" />
<xsl:template match="*">
  <xsl:copy>
     <xsl:copy-of select="@*" />
     <xsl:apply-templates />
  </xsl:copy>
</xsl:template>
<xsl:template match="comment()|processing-instruction()">
  <xsl:copy />
</xsl:template>
</xsl:stylesheet>
$$::text);

CREATE TABLE xpath_test (id integer NOT NULL, t xml);
INSERT INTO xpath_test VALUES (1, '<doc><int>1</int></doc>');
SELECT * FROM xpath_table('id', 't', 'xpath_test', '/doc/int', 'true')
as t(id int4);
SELECT * FROM xpath_table('id', 't', 'xpath_test', '/doc/int', 'true')
as t(id int4, doc int4);

DROP TABLE xpath_test;
CREATE TABLE xpath_test (id integer NOT NULL, t text);
INSERT INTO xpath_test VALUES (1, '<doc><int>1</int></doc>');
SELECT * FROM xpath_table('id', 't', 'xpath_test', '/doc/int', 'true')
as t(id int4);
SELECT * FROM xpath_table('id', 't', 'xpath_test', '/doc/int', 'true')
as t(id int4, doc int4);

create table articles (article_id integer, article_xml xml, date_entered date);
insert into articles (article_id, article_xml, date_entered)
values (2, '<article><author>test</author><pages>37</pages></article>', now());
SELECT * FROM
xpath_table('article_id',
            'article_xml',
            'articles',
            '/article/author|/article/pages|/article/title',
            'date_entered > ''2003-01-01'' ')
AS t(article_id integer, author text, page_count integer, title text);

-- this used to fail when invoked a second time
select xslt_process('<aaa/>',$$<xsl:stylesheet version="1.0"
xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:template match="@*|node()">
      <xsl:copy>
         <xsl:apply-templates select="@*|node()"/>
      </xsl:copy>
   </xsl:template>
</xsl:stylesheet>$$)::xml;

select xslt_process('<aaa/>',$$<xsl:stylesheet version="1.0"
xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:template match="@*|node()">
      <xsl:copy>
         <xsl:apply-templates select="@*|node()"/>
      </xsl:copy>
   </xsl:template>
</xsl:stylesheet>$$)::xml;

create table t1 (id integer, xml_data xml);
insert into t1 (id, xml_data)
values
(1, '<attributes><attribute name="attr_1">Some
Value</attribute></attributes>');

create index idx_xpath on t1 ( xpath_string
('/attributes/attribute[@name="attr_1"]/text()', xml_data::text));
