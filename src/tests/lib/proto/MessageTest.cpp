#include "TestSupport.hh"
#include "core/Error.hh"
#include "proto/AckResponse.hh"
#include "proto/DiagnosticRequest.hh"
#include "proto/ErrorResponse.hh"
#include "proto/GetBackupsRequest.hh"
#include "proto/GetBackupsResponse.hh"
#include "proto/Message.hh"
#include "proto/ReloadPlansRequest.hh"
#include "proto/TriggerBackupRequest.hh"

namespace anchor::test {
using namespace kc;
using namespace testing;

TEST(Message, DiagnosticRoundTripRetainsDetailsAndRuntimeType) {
    proto::DiagnosticRequest request{"line\n\"quoted\""};
    auto json = request.serialize();
    EXPECT_EQ(json["name"], "DiagnosticRequest");
    EXPECT_EQ(json["body"]["details"], request.details());
    auto restored = proto::Message::create(json);
    ASSERT_TRUE(restored);
    auto* typed = (*restored)->as<proto::DiagnosticRequest>();
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->details(), request.details());
    EXPECT_EQ((*restored)->as<proto::TriggerBackupRequest>(), nullptr);
    EXPECT_EQ((*restored)->serialize(), json);
}

TEST(Message, BackupRoundTripRetainsName) {
    proto::TriggerBackupRequest request{"daily"};
    auto restored = proto::Message::create(request.serialize());
    ASSERT_TRUE(restored);
    auto* typed = (*restored)->as<proto::TriggerBackupRequest>();
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->backupName(), "daily");
    EXPECT_EQ((*restored)->serialize(), request.serialize());
}

TEST(Message, AckRoundTripHasNullBody) {
    proto::AckResponse response;
    EXPECT_EQ(
        response.serialize(), (Json{{"name", "AckResponse"}, {"body", nullptr}})
    );
    auto restored = proto::Message::create(response.serialize());
    ASSERT_TRUE(restored);
    EXPECT_NE((*restored)->as<proto::AckResponse>(), nullptr);
}

TEST(Message, ErrorRoundTripRetainsCodeAndMessage) {
    proto::ErrorResponse response{kc::ErrorCode::fileSystemError, "disk full"};
    auto restored = proto::Message::create(response.serialize());
    ASSERT_TRUE(restored);
    auto* typed = (*restored)->as<proto::ErrorResponse>();
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->error().code(), kc::ErrorCode::fileSystemError);
    EXPECT_EQ(typed->error().message(), "disk full");
}

TEST(Message, MissingEnvelopeFieldsAndInvalidNamesReturnErrors) {
    for (const auto& json :
         {Json{}, Json::array(), Json{"scalar"}, Json{{"body", nullptr}},
          Json{{"name", "AckResponse"}},
          Json{{"name", 42}, {"body", nullptr}}}) {
        auto result = proto::Message::create(json);
        ASSERT_FALSE(result) << json.dump();
        EXPECT_EQ(result.error().code(), kc::ErrorCode::invalidArgument);
    }
    auto unknown =
        proto::Message::create(Json{{"name", "Unknown"}, {"body", nullptr}});
    ASSERT_FALSE(unknown);
    EXPECT_EQ(unknown.error().code(), ErrorCode::invalidRequest);
}

TEST(Message, EmptyNamesAndBackupNamesAreRejected) {
    auto empty = proto::Message::create(Json{{"name", ""}, {"body", nullptr}});
    ASSERT_FALSE(empty);
    EXPECT_EQ(empty.error().code(), kc::ErrorCode::invalidArgument);
    EXPECT_FALSE(
        proto::Message::create(
            Json{{"name", "TriggerBackupRequest"}, {"body", Json{{"name", ""}}}}
        )
    );
}

TEST(Message, InvalidRequestBodiesReturnErrorsInsteadOfThrowing) {
    for (const auto* name : {"DiagnosticRequest", "TriggerBackupRequest"}) {
        for (const auto& body :
             {Json{}, Json::array(), Json{{"details", 1}, {"name", false}}}) {
            auto result =
                proto::Message::create(Json{{"name", name}, {"body", body}});
            ASSERT_FALSE(result);
            EXPECT_EQ(result.error().code(), kc::ErrorCode::invalidArgument);
        }
    }
    EXPECT_FALSE(proto::DiagnosticRequest::create(Json{}));
    EXPECT_FALSE(proto::TriggerBackupRequest::create(Json{}));
}

TEST(Message, InvalidErrorBodiesReturnErrorsInsteadOfThrowing) {
    const std::vector<Json> bodies{
        Json{},
        Json::array(),
        Json{{"code", 1}},
        Json{{"message", "bad"}},
        Json{{"code", "bad"}, {"message", "bad"}},
        Json{{"code", 1}, {"message", false}},
        Json{{"code", -1}, {"message", "bad"}},
        Json{{"code", 999}, {"message", "bad"}},
        Json{{"code", 1.5}, {"message", "bad"}},
        Json{{"code", std::numeric_limits<u64>::max()}, {"message", "bad"}},
    };
    for (const auto& body : bodies) {
        auto result = proto::Message::create(
            Json{{"name", "ErrorResponse"}, {"body", body}}
        );
        ASSERT_FALSE(result) << body.dump();
        EXPECT_EQ(result.error().code(), kc::ErrorCode::invalidArgument);
    }
}

TEST(AckResponse, AcceptsOnlyNullBody) {
    EXPECT_TRUE(proto::AckResponse::create(Json{}));
    for (const auto& body :
         {Json::object(), Json::array(), Json(42), Json("text"), Json(true)}) {
        SCOPED_TRACE(body.dump());
        auto result = proto::AckResponse::create(body);
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code(), kc::ErrorCode::invalidArgument);
        EXPECT_FALSE(
            proto::Message::create(
                Json{{"name", "AckResponse"}, {"body", body}}
            )
        );
    }
}

TEST(DiagnosticRequest, ValidatesDetailsIncludingEmptyStrings) {
    for (const auto* details : {"", "details"}) {
        auto result =
            proto::DiagnosticRequest::create(Json{{"details", details}});
        ASSERT_TRUE(result);
        EXPECT_EQ((*result)->details(), details);
    }
    for (const auto& body :
         {Json{}, Json::array(), Json::object(), Json{{"details", 1}},
          Json{{"details", nullptr}}, Json{{"details", false}}}) {
        SCOPED_TRACE(body.dump());
        auto result = proto::DiagnosticRequest::create(body);
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code(), kc::ErrorCode::invalidArgument);
    }
}

TEST(ErrorResponse, RoundTripsEveryCodeInBothCategories) {
    for (const auto& [category, bounds] :
         std::vector<std::pair<Str, std::pair<int, int>>>{
             {"anchor",
              {int(ErrorCode::invalidRequest), int(ErrorCode::alreadyRunning)}},
             {"kcore", {0, int(kc::ErrorCode::notImplemented)}}
         }) {
        for (int value = bounds.first; value <= bounds.second; ++value) {
            const Json body{
                {"code", value}, {"category", category}, {"message", "details"}
            };
            auto result = proto::ErrorResponse::create(body);
            ASSERT_TRUE(result) << body;
            EXPECT_EQ((*result)->error().code().value(), value);
            EXPECT_EQ((*result)->error().code().category().name(), category);
            EXPECT_EQ((*result)->serialize()["body"], body);
        }
    }
    auto empty = proto::ErrorResponse::create(
        Json{{"category", "kcore"}, {"code", 0u}, {"message", ""}}
    );
    ASSERT_TRUE(empty);
    EXPECT_EQ((*empty)->error().message(), "");
}

TEST(ErrorResponse, RejectsMalformedCodesAndUnknownCategories) {
    const Json valid{{"category", "anchor"}, {"code", 1}, {"message", "bad"}};
    std::vector<Json> bodies{Json{}, Json::array(), Json::object()};
    for (const auto* field : {"code", "category", "message"}) {
        auto missing = valid;
        missing.erase(field);
        bodies.push_back(missing);
    }
    for (const Json& value :
         {Json("bad"), Json(-1), Json(999), Json(1.5), Json(true),
          Json(std::numeric_limits<u64>::max())}) {
        auto invalid = valid;
        invalid["code"] = value;
        bodies.push_back(invalid);
    }
    for (const Json& value : {Json(false), Json("unknown"), Json("")}) {
        auto invalid = valid;
        invalid["category"] = value;
        bodies.push_back(invalid);
    }
    auto invalidMessage = valid;
    invalidMessage["message"] = false;
    bodies.push_back(invalidMessage);
    auto zeroAnchor = valid;
    zeroAnchor["code"] = 0;
    bodies.push_back(zeroAnchor);
    auto undefinedLibrary = valid;
    undefinedLibrary["category"] = "kcore";
    undefinedLibrary["code"] = int(kc::ErrorCode::notImplemented) + 1;
    bodies.push_back(undefinedLibrary);
    for (const auto& body : bodies) {
        SCOPED_TRACE(body.dump());
        auto result = proto::ErrorResponse::create(body);
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code(), kc::ErrorCode::invalidArgument);
    }
}

TEST(TriggerBackupRequest, ValidatesNonemptyStringName) {
    auto valid = proto::TriggerBackupRequest::create(Json{{"name", "daily"}});
    ASSERT_TRUE(valid);
    EXPECT_EQ((*valid)->backupName(), "daily");
    for (const auto& body :
         {Json{}, Json::array(), Json::object(), Json{{"name", ""}},
          Json{{"name", 1}}, Json{{"name", nullptr}}, Json{{"name", false}}}) {
        SCOPED_TRACE(body.dump());
        auto result = proto::TriggerBackupRequest::create(body);
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code(), kc::ErrorCode::invalidArgument);
    }
}

TEST(Message, BodylessBackupRequestsRoundTripAndRejectUnexpectedBodies) {
    for (const auto* name : {"GetBackupsRequest", "ReloadPlansRequest"}) {
        const auto envelope = Json{{"name", name}, {"body", nullptr}};
        auto restored = proto::Message::create(envelope);
        ASSERT_TRUE(restored);
        EXPECT_EQ((*restored)->serialize(), envelope);
        for (const auto& body :
             {Json::object(), Json::array(), Json(42), Json("text")}) {
            EXPECT_FALSE(
                proto::Message::create(Json{{"name", name}, {"body", body}})
            );
        }
    }
    EXPECT_TRUE(proto::ReloadPlansRequest::create(nullptr));
    EXPECT_TRUE(proto::GetBackupsRequest::create(nullptr));
}

TEST(GetBackupsResponse, EmptyBackupsSerializeAsArrayAndRoundTrip) {
    proto::GetBackupsResponse response;
    const auto envelope = response.serialize();
    EXPECT_EQ(envelope["body"], Json::array());
    auto restored = proto::Message::create(envelope);
    ASSERT_TRUE(restored);
    const auto* typed = (*restored)->as<proto::GetBackupsResponse>();
    ASSERT_NE(typed, nullptr);
    EXPECT_TRUE(typed->backups().empty());
}

TEST(GetBackupsResponse, RoundTripRetainsEachBackupsOwnRunStateAndError) {
    std::vector<Backup> backups{Backup{.plan = plan(), .enabled = false}};
    for (const auto state :
         {BackupRunState::queued, BackupRunState::starting,
          BackupRunState::snapshotting, BackupRunState::uploading,
          BackupRunState::completed, BackupRunState::failed}) {
        auto backup = Backup{
            .plan = plan(),
            .run = BackupRunStatus{
                .state = state, .uuid = "run-" + backupRunStateToString(state)
            }
        };
        backup.plan.name = backupRunStateToString(state);
        if (state == BackupRunState::failed) {
            backup.run->error =
                Error{kc::ErrorCode::fileSystemError, "disk full"};
        }
        backups.push_back(backup);
    }
    const auto serialized = proto::GetBackupsResponse{backups}.serialize();
    auto restored = proto::Message::create(serialized);
    ASSERT_TRUE(restored);
    auto* typed = (*restored)->as<proto::GetBackupsResponse>();
    ASSERT_NE(typed, nullptr);
    ASSERT_EQ(typed->backups().size(), backups.size());
    EXPECT_FALSE(typed->backups()[0].enabled);
    EXPECT_FALSE(typed->backups()[0].run);
    for (size_t i = 1; i < backups.size(); ++i) {
        ASSERT_TRUE(typed->backups()[i].run);
        EXPECT_EQ(typed->backups()[i].plan.name, backups[i].plan.name);
        EXPECT_EQ(typed->backups()[i].run->state, backups[i].run->state);
        EXPECT_EQ(typed->backups()[i].run->uuid, backups[i].run->uuid);
    }
    ASSERT_TRUE(typed->backups().back().run->error);
    EXPECT_EQ(
        typed->backups().back().run->error->code(),
        kc::ErrorCode::fileSystemError
    );
    EXPECT_EQ(typed->backups().back().run->error->message(), "disk full");
    EXPECT_EQ((*restored)->serialize(), serialized);
}

TEST(
    GetBackupsResponse,
    MalformedBackupsAndRunStatusesReturnErrorsWithoutThrowing
) {
    const auto valid =
        Json{{"plan", Json{{"name", "daily"}}}, {"enabled", true}};
    std::vector<Json> malformed{
        nullptr,
        Json::object(),
        "text",
        42,
        Json::array({nullptr}),
        Json::array({Json::object()}),
        Json::array({Json{{"plan", Json{{"name", "daily"}}}}}),
        Json::array({Json{{"plan", Json{{"name", ""}}}, {"enabled", true}}}),
        Json::array({Json{{"plan", Json{{"name", 42}}}, {"enabled", true}}}),
        Json::array(
            {Json{{"plan", Json{{"name", "daily"}}}, {"enabled", "yes"}}}
        )
    };
    for (const auto& run :
         {Json::object(), Json::array(), Json(42), Json{{"uuid", "id"}},
          Json{{"state", "queued"}}, Json{{"uuid", "id"}, {"state", "unknown"}},
          Json{{"uuid", ""}, {"state", "queued"}},
          Json{{"uuid", 42}, {"state", "queued"}},
          Json{{"uuid", "id"}, {"state", 42}},
          Json{{"uuid", "id"}, {"state", "failed"}, {"error", Json::object()}},
          Json{
              {"uuid", "id"},
              {"state", "failed"},
              {"error", Json{{"code", 999}, {"message", "bad"}}}
          }}) {
        auto backup = valid;
        backup["run"] = run;
        malformed.push_back(Json::array({backup}));
    }
    for (const auto& body : malformed) {
        SCOPED_TRACE(body.dump());
        auto result = proto::Message::create(
            Json{{"name", "GetBackupsResponse"}, {"body", body}}
        );
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code(), kc::ErrorCode::invalidArgument);
    }
    auto withoutRun = valid;
    withoutRun["run"] = nullptr;
    EXPECT_TRUE(proto::GetBackupsResponse::create(Json::array({withoutRun})));
}

}  // namespace anchor::test
