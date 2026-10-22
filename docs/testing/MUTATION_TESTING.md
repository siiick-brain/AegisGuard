# Mutation testing status

No mutation tool or mutation run is configured. A useful first set of manual
mutations should target movement bounds, sequence ordering, credential
revocation, the token-bucket boundary and frame-length validation.

For each mutation, record whether a test kills it, whether it is equivalent and
which requirement remains uncovered if it survives. Do not treat a mutation
score as a substitute for reviewing the test assertions.
