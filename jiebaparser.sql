



CREATE FUNCTION jiebaparser_reset()
RETURNS void
AS 'MODULE_PATHNAME','jiebaparser_reset'
LANGUAGE C STRICT;

CREATE FUNCTION jbprs_start(internal, int4)
RETURNS internal
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE FUNCTION jbprs_start_q(internal, int4)
RETURNS internal
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;


CREATE FUNCTION jbprs_getlexeme(internal, internal, internal)
RETURNS internal
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE FUNCTION jbprs_end(internal)
RETURNS void
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE FUNCTION jbprs_lextype(internal)
RETURNS internal
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE TEXT SEARCH PARSER jiebaparser (
    START    = jbprs_start,
    GETTOKEN = jbprs_getlexeme,
    END      = jbprs_end,
    HEADLINE = pg_catalog.prsd_headline,
    LEXTYPES = jbprs_lextype
);

CREATE TEXT SEARCH PARSER jiebaparserqry (
    START    = jbprs_start_q,
    GETTOKEN = jbprs_getlexeme,
    END      = jbprs_end,
    HEADLINE = pg_catalog.prsd_headline,
    LEXTYPES = jbprs_lextype
);

CREATE TEXT SEARCH CONFIGURATION jbtscfg (PARSER = jiebaparser);
COMMENT ON TEXT SEARCH CONFIGURATION jbtscfg IS 'Mix segmentation configuration for jieba';

CREATE TEXT SEARCH CONFIGURATION jbqrytscfg (PARSER = jiebaparserqry);
COMMENT ON TEXT SEARCH CONFIGURATION jbqrytscfg IS 'Query segmentation configuration for jieba';


CREATE TEXT SEARCH DICTIONARY jbcfg_stem (TEMPLATE=simple);
COMMENT ON TEXT SEARCH DICTIONARY jbcfg_stem IS 'jieba dictionary: lower case and check for stopword which including Unicode symbols that are mainly Chinese characters and punctuations';


ALTER TEXT SEARCH CONFIGURATION jbtscfg ADD MAPPING FOR eng,nz,n,m,i,l,d,s,t,mq,nr,j,a,r,b,f,nrt,v,z,ns,q,vn,c,nt,u,o,zg,nrfg,df,p,g,y,ad,vg,ng,x,ul,k,ag,dg,rr,rg,an,vq,e,uv,tg,mg,ud,vi,vd,uj,uz,h,ug,rz WITH jbcfg_stem;
ALTER TEXT SEARCH CONFIGURATION jbqrytscfg ADD MAPPING FOR eng,nz,n,m,i,l,d,s,t,mq,nr,j,a,r,b,f,nrt,v,z,ns,q,vn,c,nt,u,o,zg,nrfg,df,p,g,y,ad,vg,ng,x,ul,k,ag,dg,rr,rg,an,vq,e,uv,tg,mg,ud,vi,vd,uj,uz,h,ug,rz WITH jbcfg_stem;
