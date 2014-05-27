# Test data for deps:depdiffs test.  This should be converted into an 
# xml file.
# This provides test cases for dealing with dependencies on access rights to
# the underlying schema for an object combined with access rights to
# that object when ownerships change.  Specifically, it provides for
# test cases for combinations of the following:
#
# - schema ownership changes;
# - ojbect ownership changes;
# - object is originally owned by the same/different role from the schema;
# - object becomes owned by the same/different role from the schema;
# - object owner has/has no superuser rights
# - schema has create/usage granted to public
# 
#     +-------+-------+----------------------+----------------------+---------+
# test|schema |object |create priv on schema |usage priv on schema  |superuser|
#     |owner  |owner  +-------+-------+------+-------+-------+------+   to    |
#     |role   |role   |schema |object |public|schema |object |public| owner   |
#     |       |       |owner  |owner  |      |owner  |owner  |      |         |
#     +---+---+---+---+---+---+---+---+      +---+---+---+---+      +----+----+
#     |old|new|old|new|old|new|old|new|      |old|new|old|new|      | old|new |
#     +---+---+---+---+---+---+---+---+------+---+---+---+---+------+----+----+
#     |   |   |   |   |   |   |   |   |      |   |   |   |   |      |    |    |
#   1 |r1 |r1 |r1 |r1 |yes|yes| - | - |  -   |yes|yes| - | - |  -   | -  | -  |
#     +---+---+---+---+---+---+---+---+------+---+---+---+---+------+----+----+
#   2 |r1 |r1 |r1 |r1 |yes|yes| - | - |  -   |yes|no | - | - |  no  | no | no |
#     +---+---+---+---+---+---+---+---+------+---+---+---+---+------+----+----+
#     |   |   |   |   |   |   |   |   |      |   |   |   |   |      |    |    |
#     +---+---+---+---+---+---+---+---+------+---+---+---+---+------+----+----+
#     |   |   |   |   |   |   |   |   |      |   |   |   |   |      |    |    |
#     +---+---+---+---+---+---+---+---+------+---+---+---+---+------+----+----+
#     |   |   |   |   |   |   |   |   |      |   |   |   |   |      |    |    |
#     +---+---+---+---+---+---+---+---+------+---+---+---+---+------+----+----+
#     |   |   |   |   |   |   |   |   |      |   |   |   |   |      |    |    |
#     +---+---+---+---+---+---+---+---+------+---+---+---+---+------+----+----+
#     |   |   |   |   |   |   |   |   |      |   |   |   |   |      |    |    |
#     +---+---+---+---+---+---+---+---+------+---+---+---+---+------+----+----+
#     |   |   |   |   |   |   |   |   |      |   |   |   |   |      |    |    |
#     +---+---+---+---+---+---+---+---+------+---+---+---+---+------+----+----+
#     |   |   |   |   |   |   |   |   |      |   |   |   |   |      |    |    |
#     +---+---+---+---+---+---+---+---+------+---+---+---+---+------+----+----+
#     |   |   |   |   |   |   |   |   |      |   |   |   |   |      |    |    |
#     +---+---+---+---+---+---+---+---+------+---+---+---+---+------+----+----+
#     |   |   |   |   |   |   |   |   |      |   |   |   |   |      |    |    |
#     +---+---+---+---+---+---+---+---+------+---+---+---+---+------+----+----+
#     |   |   |   |   |   |   |   |   |      |   |   |   |   |      |    |    |
#     +---+---+---+---+---+---+---+---+------+---+---+---+---+------+----+----+
#     |   |   |   |   |   |   |   |   |      |   |   |   |   |      |    |    |

# Notes on permissions required for operations:
# In order to create an object in a schema you need create permission on
# the schema.  
# In order to do any other ddl on the object you need usage permission.

psql -d postgres -v home=`pwd` <<'CLUSTEREOF'
create role r1 with login;
create role r2 with login;
create role rs with superuser;

create database regressdb with owner r1;

CLUSTEREOF

psql -d regressdb -U r1 << 'EOF'

-- Each test case has
-- a change made to the schema - comment
-- a change made to a schema object - comment
-- creation of a schema object
-- removal of a schema object

-- Test case 1
-- No ownership changes.  Schema owner has access to schema, sequence
-- owned by schema owner.
create schema n1;
create sequence n1.s1;
create sequence n1.s1a;

-- Test case 2
-- No ownership changes.  Schema owner loses usage priv on schema, sequence
-- owned by schema owner.
create schema n2;
create sequence n2.s2;
create sequence n2.s2a;

EOF