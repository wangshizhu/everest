#ifndef SESSION_CREATOR_H_
#define SESSION_CREATOR_H_

NAMESPACE_EVEREST_BEGIN

class SessionCreator : public everest::NonCopyable 
{
public:
  virtual std::shared_ptr<SessionBase> CreateSession() = 0;
};

using SessionCreatorSharedPtr = std::shared_ptr<SessionCreator>;

NAMESPACE_EVEREST_END

#endif  // !SESSION_CREATOR_H_
