-- List attributes of a given sequence


select last_value, start_value as start_with, increment_by, 
       max_value, min_value, cache_value as cache,	
       is_cycled, is_called
from   :1;