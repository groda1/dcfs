
hash_map_test = executable('hash_map_test',
    core_sources + 'hash_map_test.c',
    dependencies: dependency('criterion', required: true),
    include_directories : core_inc,
)

test('hash_map_test', hash_map_test)

array_queue_test = executable('array_queue_test',
    core_sources + 'array_queue_test.c',
    dependencies: dependency('criterion', required: true),
    include_directories : core_inc,
)

test('array_queue_test', array_queue_test)
