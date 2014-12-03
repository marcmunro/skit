select name, setting, source, 
       context, sourcefile, sourceline
  from pg_settings
 where source != 'default';
