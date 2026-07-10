cmake_minimum_required(VERSION 3.10)
project(banking_backend CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find necessary libraries
find_package(Drogon REQUIRED)
find_package(OpenSSL REQUIRED)

# Find PostgreSQL
find_path(PQ_INCLUDE_DIR libpq-fe.h PATH_SUFFIXES postgresql)
find_library(PQ_LIBRARY NAMES pq)
if(NOT PQ_INCLUDE_DIR OR NOT PQ_LIBRARY)
    message(FATAL_ERROR "PostgreSQL client library (libpq) not found!")
endif()

# Find Hiredis (Redis)
find_path(HIREDIS_INCLUDE_DIR hiredis/hiredis.h)
find_library(HIREDIS_LIBRARY NAMES hiredis)
if(NOT HIREDIS_INCLUDE_DIR OR NOT HIREDIS_LIBRARY)
    message(FATAL_ERROR "Hiredis library not found!")
endif()

# Find Argon2
find_path(ARGON2_INCLUDE_DIR argon2.h)
find_library(ARGON2_LIBRARY NAMES argon2)
if(NOT ARGON2_INCLUDE_DIR OR NOT ARGON2_LIBRARY)
    message(FATAL_ERROR "Argon2 library not found!")
endif()

# Find GTest
find_package(GTest)

# Source files list
set(SOURCE_FILES
    src/main.cc
    src/security/Argon2Hasher.cc
    src/security/TokenManager.cc
    src/security/RateLimiter.cc
    src/security/EncryptionUtil.cc
    src/services/LedgerService.cc
    src/services/LoanEngine.cc
    src/services/FixedDepositManager.cc
    src/services/NigerianUtilityProviders.cc
    src/services/MerchantManager.cc
    src/services/NotificationService.cc
    src/controllers/AuthController.cc
    src/controllers/AccountController.cc
    src/controllers/TransactionController.cc
    src/controllers/LoanController.cc
    src/controllers/FixedDepositController.cc
    src/controllers/BillController.cc
    src/controllers/MerchantController.cc
    src/controllers/AdminController.cc
    src/middleware/AuthMiddleware.cc
    src/middleware/RateLimitMiddleware.cc
    src/middleware/IdempotencyMiddleware.cc
)

# Main Executable
add_executable(banking_backend ${SOURCE_FILES})

# Include Directories
target_include_directories(banking_backend PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${PQ_INCLUDE_DIR}
    ${HIREDIS_INCLUDE_DIR}
    ${OPENSSL_INCLUDE_DIR}
    ${ARGON2_INCLUDE_DIR}
)

# Link Libraries
target_link_libraries(banking_backend PRIVATE
    Drogon::Drogon
    ${PQ_LIBRARY}
    ${HIREDIS_LIBRARY}
    ${ARGON2_LIBRARY}
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Setup Tests if GTest is found
if(GTest_FOUND)
    enable_testing()
    
    set(TEST_SOURCE_FILES
        tests/main.cc
        tests/ledger_reconciliation_test.cc
        tests/idempotency_test.cc
        tests/loan_rule_test.cc
        tests/concurrency_test.cc
        src/security/Argon2Hasher.cc
        src/security/TokenManager.cc
        src/security/RateLimiter.cc
        src/security/EncryptionUtil.cc
        src/services/LedgerService.cc
        src/services/LoanEngine.cc
        src/services/FixedDepositManager.cc
        src/services/NigerianUtilityProviders.cc
        src/services/MerchantManager.cc
        src/services/NotificationService.cc
        src/middleware/AuthMiddleware.cc
        src/middleware/RateLimitMiddleware.cc
        src/middleware/IdempotencyMiddleware.cc
    )
    
    add_executable(backend_tests ${TEST_SOURCE_FILES})
    
    target_include_directories(backend_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${PQ_INCLUDE_DIR}
        ${HIREDIS_INCLUDE_DIR}
        ${OPENSSL_INCLUDE_DIR}
        ${ARGON2_INCLUDE_DIR}
    )
    
    target_link_libraries(backend_tests PRIVATE
        Drogon::Drogon
        ${PQ_LIBRARY}
        ${HIREDIS_LIBRARY}
        ${ARGON2_LIBRARY}
        OpenSSL::SSL
        OpenSSL::Crypto
        GTest::GTest
        GTest::Main
    )
    
    add_test(NAME backend_tests COMMAND backend_tests)
endif()
