-- List all roles assigned to each role
select role.rolname as priv,
       member.rolname as "to",
       grantor.rolname as "from",
       case when a.admin_option then 'yes' else 'no' end as with_admin
from   pg_catalog.pg_auth_members a,
       pg_catalog.pg_authid role,
       pg_catalog.pg_authid member,
       pg_catalog.pg_authid grantor
where  role.oid = a.roleid
and    member.oid = a.member
and    grantor.oid = a.grantor
order by 1,2;
