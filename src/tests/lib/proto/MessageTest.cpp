#include "TestSupport.hh"
#include "proto/AckResponse.hh"
#include "proto/DiagnosticRequest.hh"
#include "proto/ErrorResponse.hh"
#include "proto/Message.hh"
#include "proto/TriggerBackupRequest.hh"

namespace anchor::test {
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
    proto::ErrorResponse response{ErrorCode::fileSystemError, "disk full"};
    auto restored = proto::Message::create(response.serialize());
    ASSERT_TRUE(restored);
    auto* typed = (*restored)->as<proto::ErrorResponse>();
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->error().code(), ErrorCode::fileSystemError);
    EXPECT_EQ(typed->error().message(), "disk full");
}

TEST(Message, MissingEnvelopeFieldsAndInvalidNamesReturnErrors) {
    for (const auto& json :
         {Json{}, Json::array(), Json{"scalar"}, Json{{"body", nullptr}},
          Json{{"name", "AckResponse"}},
          Json{{"name", 42}, {"body", nullptr}}}) {
        auto result = proto::Message::create(json);
        ASSERT_FALSE(result) << json.dump();
        EXPECT_EQ(result.error().code(), ErrorCode::invalidArgument);
    }
    auto unknown =
        proto::Message::create(Json{{"name", "Unknown"}, {"body", nullptr}});
    ASSERT_FALSE(unknown);
    EXPECT_EQ(unknown.error().code(), ErrorCode::invalidRequest);
}

TEST(Message, EmptyNamesAndBackupNamesAreRejected) {
    auto empty = proto::Message::create(Json{{"name", ""}, {"body", nullptr}});
    ASSERT_FALSE(empty);
    EXPECT_EQ(empty.error().code(), ErrorCode::invalidArgument);
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
            EXPECT_EQ(result.error().code(), ErrorCode::invalidArgument);
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
        EXPECT_EQ(result.error().code(), ErrorCode::invalidArgument);
    }
}

TEST(AckResponse, AcceptsOnlyNullBody) {
    EXPECT_TRUE(proto::AckResponse::create(Json{}));
    for (const auto& body :
         {Json::object(), Json::array(), Json(42), Json("text"), Json(true)}) {
        SCOPED_TRACE(body.dump());
        auto result = proto::AckResponse::create(body);
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code(), ErrorCode::invalidArgument);
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
        EXPECT_EQ(result.error().code(), ErrorCode::invalidArgument);
    }
}

TEST(ErrorResponse, AcceptsEveryDefinedErrorCodeWithStringMessage) {
    for (int code = 0;
         code <= std::to_underlying(ErrorCode::serializationFailure); ++code) {
        auto result = proto::ErrorResponse::create(
            Json{{"code", code}, {"message", "details"}}
        );
        ASSERT_TRUE(result);
        EXPECT_EQ(std::to_underlying((*result)->error().code()), code);
        EXPECT_EQ((*result)->error().message(), "details");
        EXPECT_EQ(
            (*result)->serialize()["body"],
            (Json{{"code", code}, {"message", "details"}})
        );
    }
    auto empty =
        proto::ErrorResponse::create(Json{{"code", 0u}, {"message", ""}});
    ASSERT_TRUE(empty);
    EXPECT_EQ((*empty)->error().message(), "");
}

TEST(ErrorResponse, RejectsMissingFieldsWrongTypesAndUndefinedErrorCodes) {
    const std::vector<Json> bodies{
        Json{},
        Json::array(),
        Json::object(),
        Json{{"code", 1}},
        Json{{"message", "bad"}},
        Json{{"code", "bad"}, {"message", "bad"}},
        Json{{"code", 1}, {"message", false}},
        Json{{"code", -1}, {"message", "bad"}},
        Json{{"code", 999}, {"message", "bad"}},
        Json{{"code", 1.5}, {"message", "bad"}},
        Json{{"code", true}, {"message", "bad"}},
        Json{{"code", std::numeric_limits<u64>::max()}, {"message", "bad"}},
    };
    for (const auto& body : bodies) {
        SCOPED_TRACE(body.dump());
        auto result = proto::ErrorResponse::create(body);
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code(), ErrorCode::invalidArgument);
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
        EXPECT_EQ(result.error().code(), ErrorCode::invalidArgument);
    }
}

}  // namespace anchor::test
