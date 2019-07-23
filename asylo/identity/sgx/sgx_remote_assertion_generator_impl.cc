/*
 *
 * Copyright 2019 Asylo authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include "asylo/identity/sgx/sgx_remote_assertion_generator_impl.h"

#include <memory>
#include <vector>

#include "asylo/grpc/auth/peer_identity_util.h"
#include "asylo/identity/sgx/code_identity_util.h"
#include "asylo/identity/sgx/remote_assertion_util.h"
#include "asylo/util/mutex_guarded.h"
#include "asylo/util/status.h"
#include "include/grpcpp/support/status.h"

namespace asylo {

SgxRemoteAssertionGeneratorImpl::SgxRemoteAssertionGeneratorImpl(
    std::unique_ptr<SigningKey> signing_key,
    const std::vector<CertificateChain> &certificate_chains)
    : signing_key_(std::move(signing_key)),
      certificate_chains_(certificate_chains) {}

::grpc::Status SgxRemoteAssertionGeneratorImpl::GenerateSgxRemoteAssertion(
    ::grpc::ServerContext *context,
    const GenerateSgxRemoteAssertionRequest *request,
    GenerateSgxRemoteAssertionResponse *response) {
  sgx::CodeIdentity code_identity;
  Status status = ExtractAndCheckPeerSgxCodeIdentity(*context->auth_context(),
                                                     &code_identity);
  if (!status.ok()) {
    return status.ToOtherStatus<::grpc::Status>();
  }
  auto signing_key_locked = signing_key_.ReaderLock();
  auto certificate_chains_locked = certificate_chains_.ReaderLock();
  status = MakeRemoteAssertion(request->user_data(), code_identity,
                               **signing_key_locked, *certificate_chains_locked,
                               response->mutable_assertion());
  if (!status.ok()) {
    LOG(ERROR) << "MakeRemoteAssertion failed: " << status;
    return ::grpc::Status(::grpc::StatusCode::INTERNAL,
                          "Failed to generate SGX remote assertion");
  }
  return ::grpc::Status::OK;
}

void SgxRemoteAssertionGeneratorImpl::UpdateSigningKeyAndCertificateChains(
    std::unique_ptr<SigningKey> signing_key,
    const std::vector<CertificateChain> &certificate_chains) {
  auto signing_key_locked = signing_key_.Lock();
  auto certificate_chains_locked = certificate_chains_.Lock();
  *signing_key_locked = std::move(signing_key);
  *certificate_chains_locked = certificate_chains;
}

}  // namespace asylo